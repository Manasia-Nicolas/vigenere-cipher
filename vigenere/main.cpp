#include <string>
#include <iostream>
#include <fstream>
#include <vector>

class VignereCypher{
private:
    std::string key;
    static int ALFABET_LENGTH;

    static char helper_increase(char a, char b){
        return (a - 'A' + b - 'A') % ALFABET_LENGTH + 'A';
    }
    static char helper_decrease(char a, char b){
        return (a - b + ALFABET_LENGTH) % ALFABET_LENGTH + 'A';
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
    // between them is a multiple of the key length.
    static int FindLength(const std::string& encoded){
        std::vector<int> distances;

        for(int i = 0; i < (int)encoded.size() - 3; ++i) {
            std::string tri = encoded.substr(i, 3);
            for(int j = i + 1; j < (int)encoded.size() - 3; ++j)
                if(encoded.substr(j, 3) == tri)
                    distances.push_back(j - i);
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

    // Builds the English letter distribution by counting the letters of
    // every word in the dictionary.
    static std::vector<double> LetterFrequencies(const std::vector<std::string>& words){
        std::vector<long long> count(ALFABET_LENGTH, 0);
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

        std::vector<double> freq(ALFABET_LENGTH);
        for(int i = 0; i < ALFABET_LENGTH; ++i)
            freq[i] = (double)count[i] / total;
        return freq;
    }

    // Frequency analysis: every i-th letter (mod key length) is a Caesar
    // cipher with the same shift. For each of the ALFABET_LENGTH possible shifts, compare
    // the resulting letter distribution with English letter frequencies
    // (chi-squared) and keep the shift that matches best.
    static std::string FrequentAnalysis(const std::string& encoded, int length,
                                        const std::vector<double>& english_freq){
        std::string actual_key(length, ' ');

        for(int i = 0; i < length; ++i) {
            int freq[ALFABET_LENGTH] = {};
            int n = 0;
            for(int j = i; j < (int)encoded.size(); j += length) {
                ++freq[encoded[j] - 'A'];
                ++n;
            }

            int best_shift = 0;
            double best_chi2 = -1;
            for(int shift = 0; shift < ALFABET_LENGTH; ++shift) {
                double chi2 = 0;
                for(int c = 0; c < ALFABET_LENGTH; ++c) {
                    double expected = english_freq[c] * n;
                    double observed = freq[(c + shift) % ALFABET_LENGTH];
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
