#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_set>


// The set of symbols the cipher operates on. The first 26 symbols are always
// A-Z; a language may add extra symbols (the German umlauts and eszett),
// which are multi-byte UTF-8 sequences in the text. Internally every symbol
// is represented as the single char 'A' + index, so the rest of the cipher
// can keep working on plain std::string.
class Alphabet{
private:
      // extra symbols beyond A-Z as (uppercase, lowercase) UTF-8 pairs;
    // the symbol of extras[i] is 'A' + 26 + i
    std::vector<std::string> extras;

    // the language's letter distribution, filled by LoadFrequencies
    std::vector<double> frequencies;

public:
    int size() const {
        return 26 + (int)extras.size();
    }
    
    // Reads the symbol starting at byte position i (folding lowercase to
    // uppercase) and advances i past it. Returns the internal char, or 0
    // (advancing one byte) if no alphabet symbol starts there.
    char SymbolToChar(const std::string& text, int& i) const {
        char c = text[i];
        if(c >= 'A' && c <= 'Z'){ ++i; return c; }
        for(int e = 0; e < (int)extras.size(); ++e){
            const auto& character = extras[e];
            if(text.compare(i, character.size(), character) == 0){ i += character.size(); return (char)('A' + 26 + e); }
        }
        ++i;
        return 0;
    }

    std::string CharToSymbol(char ch) const {
        if(ch < 'A' + 26)
            return std::string(1, ch);
        return extras[ch - 'A' - 26];
    }

    static Alphabet English(){
        return Alphabet{};
    }

    static Alphabet German(){
        Alphabet a;
        a.extras = {"Ä", "Ö", "Ü", "ß"};
        return a;
    }

    // Converts text to the internal one-char-per-CharToSymbol representation,
    // dropping everything that is not an alphabet CharToSymbol.
    static std::string ExtractLetters(const std::string& text, const Alphabet& alphabet){
        std::string letters;
        int i = 0;
        while(i < (int)text.size()){
            char c = alphabet.SymbolToChar(text, i);
            if(c != 0)
                letters += c;
        }
        return letters;
    }

    // Builds the reference letter distribution of the language by counting
    // the symbols of every word in the dictionary.
    std::vector<double> LetterFrequencies(const std::vector<std::string>& words) const {
        std::vector<long long> count(size(), 0);
        long long total = 0;
        for(const std::string& word : words) {
            int i = 0;
            while(i < (int)word.size()){
                char c = SymbolToChar(word, i);
                if(c != 0){
                    ++count[c - 'A'];
                    ++total;
                }
            }
        }

        std::vector<double> freq(size());
        for(int i = 0; i < size(); ++i)
            freq[i] = (double)count[i] / total;
        return freq;
    }

    const std::vector<double>& Frequencies() const {
        return frequencies;
    }

    // Fills the frequencies member from the cache file if it is valid for
    // this alphabet (one value per symbol, summing to ~1). Otherwise falls
    // back to counting the dictionary words — the only O(dictionary) path —
    // and rewrites the cache for the next run.
    void LoadFrequencies(const std::string& cache_file, const std::vector<std::string>& words){
        std::ifstream in(cache_file);
        std::vector<double> cached(size());
        int loaded = 0;
        double value, sum = 0;
        while(loaded < size() && in >> value){
            cached[loaded++] = value;
            sum += value;
        }

        bool valid = loaded == size() && sum > 0.99 && sum < 1.01
                     && !(in >> value);   // extra values = file for another alphabet
        if(valid){
            frequencies = cached;
            return;
        }

        frequencies = LetterFrequencies(words);
        std::ofstream out(cache_file);
        out.precision(12);
        for(double f : frequencies)
            out << f << "\n";
    }
};

class VignereCypher{
private:
    Alphabet alphabet;
    std::string key;           // internal chars, one per key Symbol
    static const int MAX_KEY_LENGTH = 10;


    char helper_increase(char a, char b) const {
        return (a - 'A' + b - 'A') % alphabet.size() + 'A';
    }
    char helper_decrease(char a, char b) const {
        return (a - b + alphabet.size()) % alphabet.size() + 'A';
    }
    // Kasiski examination: repeated trigrams in the ciphertext are likely the
    // same plaintext encrypted with the same key position, so the distance
    // between them is a multiple of the key length.
    static int FindLengthKasiski(const std::string& encoded, int n_symbols){
        std::vector<int> distances;
        std::vector<int> last_pos(n_symbols * n_symbols * n_symbols, -1);

        for(int i = 0; i + 3 <= (int)encoded.size(); ++i) {
            int code = ((encoded[i]     - 'A') * n_symbols
                    + (encoded[i + 1] - 'A')) * n_symbols
                    + (encoded[i + 2] - 'A');
            if(last_pos[code] != -1)
                distances.push_back(i - last_pos[code]);
            last_pos[code] = i;
         }

        if(distances.empty())
            return 1;

        // Score each candidate length k by (distances divisible by k) * k.
        // A coincidental repeat is divisible by k with probability 1/k, so
        // noise contributes the same expected amount to every candidate,
        // while the true key length stands out. Multiples of the true
        // length tie with it (every distance they catch, it catches too),
        // so take the SMALLEST candidate close to the maximum score.
        std::vector<long long> score(MAX_KEY_LENGTH + 1, 0);
        for(int k = 1; k <= MAX_KEY_LENGTH; ++k) {
            long long count = 0;
            for(int d : distances)
                if(d % k == 0)
                    ++count;
            score[k] = count * k;
        }

        long long best_score = 0;
        for(int k = 1; k <= MAX_KEY_LENGTH; ++k)
            best_score = std::max(best_score, score[k]);

        for(int k = 1; k <= MAX_KEY_LENGTH; ++k)
            if(score[k] * 10 >= best_score * 9)
                return k;

        return 1;
    }

    // Friedman test (index of coincidence): the IC of a text is the
    // probability that two randomly picked symbols are equal,
    //     IC = sum_c f_c * (f_c - 1) / (n * (n - 1)).
    // Natural language has a high IC (~0.067 for English) because letter
    // frequencies are skewed; a polyalphabetic ciphertext looks flatter
    // (~1/alphabet_size). Splitting by the CORRECT key length puts every
    // column under a single Caesar shift, and since a shift only relabels
    // symbols, each column's IC jumps back up to the language's value.
    // Wrong lengths mix several shifts per column and the IC stays low.
    static int FindLengthFriedman(const std::string& encoded, int n_symbols){
    std::vector<double> avg_ic(MAX_KEY_LENGTH + 1, 0);

    for(int k = 1; k <= MAX_KEY_LENGTH; ++k) {
        double ic_sum = 0;
        int columns_used = 0;

        for(int i = 0; i < k; ++i) {
            std::vector<long long> freq(n_symbols, 0);
            long long n = 0;
            for(int j = i; j < (int)encoded.size(); j += k) {
                ++freq[encoded[j] - 'A'];
                ++n;
            }
            if(n < 2)
                continue;  // column too short to define an IC

            double ic = 0;
            for(int c = 0; c < n_symbols; ++c)
                ic += (double)freq[c] * (freq[c] - 1);
            ic /= (double)n * (n - 1);

            ic_sum += ic;
            ++columns_used;
        }

        if(columns_used > 0)
            avg_ic[k] = ic_sum / columns_used;
    }

    // The true length maximizes the average column IC, but its multiples
    // score just as high (their columns are also single-shift, only
    // shorter and noisier). So, as in Kasiski, take the SMALLEST length
    // whose score is close to the maximum.
    double best_ic = 0;
    for(int k = 1; k <= MAX_KEY_LENGTH; ++k)
        best_ic = std::max(best_ic, avg_ic[k]);

    for(int k = 1; k <= MAX_KEY_LENGTH; ++k)
        if(avg_ic[k] * 10 >= best_ic * 9)
            return k;

    return 1;
    }

    // Frequency analysis: every i-th letter (mod key length) is a Caesar
    // cipher with the same shift. For each possible shift, compare the
    // resulting letter distribution with the language's letter frequencies
    // (chi-squared) and keep the shift that matches best.
    static std::string FrequentAnalysis(const std::string& encoded, int length,
                                        const std::vector<double>& language_freq){
        int n_symbols = (int)language_freq.size();
        std::string actual_key(length, ' ');

        for(int i = 0; i < length; ++i) {
            std::vector<int> freq(n_symbols, 0);
            int n = 0;
            for(int j = i; j < (int)encoded.size(); j += length) {
                ++freq[encoded[j] - 'A'];
                ++n;
            }

            int best_shift = 0;
            double best_chi2 = -1;
            for(int shift = 0; shift < n_symbols; ++shift) {
                double chi2 = 0;
                for(int c = 0; c < n_symbols; ++c) {
                    double expected = language_freq[c] * n;
                    double observed = freq[(c + shift) % n_symbols];
                    if(expected > 0)
                        chi2 += (observed - expected) * (observed - expected) / expected;
                    else if(observed > 0)
                        chi2 += 1e9;  // CharToSymbol never occurs in this language
                }
                if(best_chi2 < 0 || chi2 < best_chi2) {
                    best_chi2 = chi2;
                    best_shift = shift;
                }
            }
            actual_key[i] = (char)('A' + best_shift);
        }
        return actual_key;
    }

    // Converts an internal key to its text form.
    static std::string KeyToText(const std::string& internal_key, const Alphabet& alphabet){
        std::string key_text;
        for(char c : internal_key)
            key_text += alphabet.CharToSymbol(c);
        return key_text;
    }

    // Counts how many of the text's words appear in the dictionary —
    // a measure of how "readable" a candidate decryption is.
    static long long CountDictionaryWords(const std::string& text,
                                          const std::unordered_set<std::string>& dictionary,
                                          const Alphabet& alphabet){
        long long count = 0;
        std::string word;
        int i = 0;
        while(i < (int)text.size()){
            char c = alphabet.SymbolToChar(text, i);
            if(c != 0){
                word += alphabet.CharToSymbol(c);
            } else {
                if(!word.empty() && dictionary.count(word))
                    ++count;
                word.clear();
            }
        }
        if(!word.empty() && dictionary.count(word))
            ++count;
        return count;
    }

public:
    VignereCypher(const std::string& _key, Alphabet _alphabet)
        : alphabet(std::move(_alphabet)) {
        key = Alphabet::ExtractLetters(_key, alphabet);
        if(key.empty())
            throw std::runtime_error("Key contains no alphabet symbols");
    }

    // Recovers the key from a ciphertext and returns a cypher ready to
    // decode it. The key length is estimated with both the Kasiski
    // examination and the Friedman test; when they disagree (mostly on
    // short texts), the key is derived for both lengths and the one whose
    // decryption contains more dictionary words wins. The dictionary is
    // also used to build the language's reference letter distribution.
    static VignereCypher Crack(const std::string& encoded,
                               const std::vector<std::string>& words,
                               const Alphabet& alphabet){
        std::string letters = Alphabet::ExtractLetters(encoded, alphabet);
        std::vector<double> language_freq = alphabet.Frequencies().empty()
            ? alphabet.LetterFrequencies(words)   // LoadFrequencies was never called
            : alphabet.Frequencies();

        int len_kasiski = FindLengthKasiski(letters, alphabet.size());
        int len_friedman = FindLengthFriedman(letters, alphabet.size());

        std::string key_kasiski =
            KeyToText(FrequentAnalysis(letters, len_kasiski, language_freq), alphabet);
        if(len_kasiski == len_friedman)
            return VignereCypher(key_kasiski, alphabet);

        std::string key_friedman =
            KeyToText(FrequentAnalysis(letters, len_friedman, language_freq), alphabet);

        VignereCypher by_kasiski(key_kasiski, alphabet);
        VignereCypher by_friedman(key_friedman, alphabet);

        std::unordered_set<std::string> dictionary(words.begin(), words.end());
        long long words_kasiski = CountDictionaryWords(by_kasiski.decode(encoded), dictionary, alphabet);
        long long words_friedman = CountDictionaryWords(by_friedman.decode(encoded), dictionary, alphabet);

        return words_kasiski >= words_friedman ? by_kasiski : by_friedman;
    }

    std::string GetKey() const {
        return KeyToText(key, alphabet);
    }

    std::string encode(const std::string& message) const {
        std::string out;
        int i = 0, key_pos = 0;
        while(i < (int)message.size()){
            int start = i;
            char c = alphabet.SymbolToChar(message, i);
            if(c == 0){
                out.append(message, start, i - start);
            } else {
                out += alphabet.CharToSymbol(helper_increase(c, key[key_pos % key.size()]));
                ++key_pos;
            }
        }
        return out;
    }

    std::string encode(const std::string& message, const std::string& other_key) const {
        return VignereCypher(other_key, alphabet).encode(message);
    }

    std::string decode(const std::string& message) const {
        std::string out;
        int i = 0, key_pos = 0;
        while(i < (int)message.size()){
            int start = i;
            char c = alphabet.SymbolToChar(message, i);
            if(c == 0){
                out.append(message, start, i - start);
            } else {
                out += alphabet.CharToSymbol(helper_decrease(c, key[key_pos % key.size()]));
                ++key_pos;
            }
        }
        return out;
    }
};

int main(int argc, char* argv[]) {
    // usage: ./vigenere [en|de] [encrypted-file]
    std::string lang = argc > 1 ? argv[1] : "en";
    std::string input_file = argc > 2 ? argv[2] : "vigenere/long-encrypted.txt";

    Alphabet alphabet = lang == "de" ? Alphabet::German() : Alphabet::English();
    std::string dict_file = lang == "de" ? "vigenere/wordlist-de_DE/de_DE.txt"
                                         : "vigenere/wordlist-en_US/en_US.txt";

    std::string input_encoded;
    std::ifstream get_input(input_file);
    if(!get_input){
        throw std::runtime_error("Input file is not opened");
    }
    std::string line;
    while(std::getline(get_input, line))
        input_encoded += line + " ";

    std::ifstream wordList(dict_file);
    if(!wordList){
        throw std::runtime_error("WordList is not opened");
    }
    std::string read;
    std::vector<std::string> activeWordList;
    while(wordList >> read){
        activeWordList.push_back(read);
    }

    std::string freq_file = lang == "de" ? "vigenere/wordlist-de_DE/frequencies.txt"
                                         : "vigenere/wordlist-en_US/frequencies.txt";
    alphabet.LoadFrequencies(freq_file, activeWordList);

    VignereCypher VC = VignereCypher::Crack(input_encoded, activeWordList, alphabet);

    std::cout << "1." << VC.GetKey() << "\n";
    std::cout << "2." << VC.decode(input_encoded) << '\n';

    return 0;
}
