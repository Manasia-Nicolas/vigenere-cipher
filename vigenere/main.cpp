#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>

class VignereCypher{
private:
    std::string key;

    static char helper_increase(char a, char b){
        return (a - 'A' + b - 'A') % 26 + 'A';
    }
    static char helper_decrease(char a, char b){
        return (a - b + 26) % 26 + 'A';
    }

    static std::string ExtractLetters(const std::string& text){
        std::string letters;
        for(char c : text)
            if(c >= 'A' && c <= 'Z')
                letters += c;
        return letters;
    }

    static const int MAX_KEY_LENGTH = 10;

    // Kasiski examination: repeated trigrams in the ciphertext are likely the
    // same plaintext encrypted with the same key position, so the distance
    // between them is a multiple of the key length. The key is at most
    // MAX_KEY_LENGTH chars, so we only count the factors up to that limit.
    static int FindLength(const std::string& encoded){
        std::map<int, int> factor_count;

        for(int i = 0; i < (int)encoded.size() - 3; ++i) {
            std::string tri = encoded.substr(i, 3);
            for(int j = i + 1; j < (int)encoded.size() - 3; ++j) {
                if(encoded.substr(j, 3) == tri) {
                    int dist = j - i;
                    for(int f = 2; f <= MAX_KEY_LENGTH && f <= dist; ++f)
                        if(dist % f == 0)
                            ++factor_count[f];
                }
            }
        }

        if(factor_count.empty())
            return 1;

        int best_count = 0;
        for(auto& [f, cnt] : factor_count)
            if(cnt > best_count) best_count = cnt;

        // If the true length is 4, factor 2 divides every true distance as
        // well, so its count is at least as high. Pick the LARGEST factor
        // whose count is close to the maximum (coincidental repeats only
        // add noise).
        int best = 1;
        for(auto& [f, cnt] : factor_count)
            if(cnt * 10 >= best_count * 9)
                best = std::max(best, f);

        return best;
    }

    // Builds the English letter distribution by counting the letters of
    // every word in the dictionary.
    static std::vector<double> LetterFrequencies(const std::vector<std::string>& words){
        std::vector<long long> count(26, 0);
        long long total = 0;
        for(const std::string& word : words) {
            for(char c : word) {
                if(c >= 'a' && c <= 'z')
                    c = c - 'a' + 'A';
                if(c >= 'A' && c <= 'Z') {
                    ++count[c - 'A'];
                    ++total;
                }
            }
        }

        std::vector<double> freq(26);
        for(int i = 0; i < 26; ++i)
            freq[i] = (double)count[i] / total;
        return freq;
    }

    // Frequency analysis: every i-th letter (mod key length) is a Caesar
    // cipher with the same shift. For each of the 26 possible shifts, compare
    // the resulting letter distribution with English letter frequencies
    // (chi-squared) and keep the shift that matches best.
    static std::string FrequentAnalysis(const std::string& encoded, int length,
                                        const std::vector<double>& english_freq){
        std::string actual_key(length, ' ');

        for(int i = 0; i < length; ++i) {
            int freq[26] = {};
            int n = 0;
            for(int j = i; j < (int)encoded.size(); j += length) {
                ++freq[encoded[j] - 'A'];
                ++n;
            }

            int best_shift = 0;
            double best_chi2 = -1;
            for(int shift = 0; shift < 26; ++shift) {
                double chi2 = 0;
                for(int c = 0; c < 26; ++c) {
                    double expected = english_freq[c] * n;
                    double observed = freq[(c + shift) % 26];
                    chi2 += (observed - expected) * (observed - expected) / expected;
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

public:
    explicit VignereCypher(std::string _key) : key(std::move(_key)) {}

    // Recovers the key from a ciphertext (Kasiski + frequency analysis)
    // and returns a cypher ready to decode it. The dictionary words are
    // used to build the reference English letter distribution.
    static VignereCypher Crack(const std::string& encoded,
                               const std::vector<std::string>& words){
        std::string letters = ExtractLetters(encoded);
        int length = FindLength(letters);
        std::vector<double> english_freq = LetterFrequencies(words);
        return VignereCypher(FrequentAnalysis(letters, length, english_freq));
    }

    const std::string& GetKey() const {
        return key;
    }

    std::string encode(std::string message) const {
        int key_pos = 0;
        for(int i = 0; i < (int)message.size(); ++i) {
            if(message[i] >= 'A' && message[i] <= 'Z') {
                message[i] = helper_increase(message[i], key[key_pos % key.size()]);
                ++key_pos;
            }
        }
        return message;
    }

    std::string encode(std::string message, std::string key){
        int key_pos = 0;
        for(int i = 0; i < (int)message.size(); ++i) {
            if(message[i] >= 'A' && message[i] <= 'Z') {
                message[i] = helper_increase(message[i], key[key_pos % key.size()]);
                ++key_pos;
            }
        }
        return message;

    }

    std::string decode(std::string message) const {
        int key_pos = 0;
        for(int i = 0; i < (int)message.size(); ++i) {
            if(message[i] >= 'A' && message[i] <= 'Z') {
                message[i] = helper_decrease(message[i], key[key_pos % key.size()]);
                ++key_pos;
            }
        }
        return message;
    }
};

int main() {
    std::string input_encoded;
    std::ifstream get_input("vigenere/long-encrypted.txt");
    std::string line;
    while(std::getline(get_input, line))
        input_encoded += line + " ";

    std::ifstream wordList("vigenere/wordlist-en_US/en_US.txt");
    if(!wordList){
        throw std::runtime_error("WordList is not opened");
    }
    std::string read;
    std::vector<std::string> activeWordList;
    while(wordList >> read){
        activeWordList.push_back(read);
    }

    VignereCypher VC = VignereCypher::Crack(input_encoded, activeWordList);

    std::cout << "1." << VC.GetKey() << "\n";
    std::cout << "2." << VC.decode(input_encoded) << '\n';

    return 0;
}
