#pragma once

#include "alphabet.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

class VigenereCypher {
  private:
    Alphabet alphabet;
    std::string key; // internal chars, one per key symbol

    char helper_increase(char a, char b) const {
        return (a - 'A' + b - 'A') % alphabet.size() + 'A';
    }
    char helper_decrease(char a, char b) const {
        return (a - b + alphabet.size()) % alphabet.size() + 'A';
    }

  public:
    static const int MAX_KEY_LENGTH = 10;

    // ----- building blocks of Crack -------------------------------------
    // Public so unit tests and benchmarks can exercise them directly.

    // Kasiski examination: repeated trigrams in the ciphertext are likely the
    // same plaintext encrypted with the same key position, so the distance
    // between them is a multiple of the key length.
    static int FindLengthKasiski(const std::string& encoded, int n_symbols) {
        std::vector<int> distances;
        std::vector<int> last_pos(n_symbols * n_symbols * n_symbols, -1);

        for (int i = 0; i + 3 <= (int)encoded.size(); ++i) {
            int code = ((encoded[i] - 'A') * n_symbols + (encoded[i + 1] - 'A')) * n_symbols +
                       (encoded[i + 2] - 'A');
            if (last_pos[code] != -1) distances.push_back(i - last_pos[code]);
            last_pos[code] = i;
        }

        if (distances.empty()) return 1;

        // Score each candidate length k by (distances divisible by k) * k.
        // A coincidental repeat is divisible by k with probability 1/k, so
        // noise contributes the same expected amount to every candidate,
        // while the true key length stands out. Multiples of the true
        // length tie with it (every distance they catch, it catches too),
        // so take the SMALLEST candidate close to the maximum score.
        std::vector<long long> score(MAX_KEY_LENGTH + 1, 0);
        for (int k = 1; k <= MAX_KEY_LENGTH; ++k) {
            long long count = 0;
            for (int d : distances)
                if (d % k == 0) ++count;
            score[k] = count * k;
        }

        long long best_score = 0;
        for (int k = 1; k <= MAX_KEY_LENGTH; ++k)
            best_score = std::max(best_score, score[k]);

        for (int k = 1; k <= MAX_KEY_LENGTH; ++k)
            if (score[k] * 10 >= best_score * 9) return k;

        return 1;
    }

    // Friedman test (index of coincidence): the IC of a text is the
    // probability that two randomly picked symbols are equal,
    //     IC = sum_c f_c * (f_c - 1) / (n * (n - 1)).
    // The expected IC is derived from the selected language's frequencies:
    //     expected_IC = sum_c p_c^2.
    // Splitting by the CORRECT key length puts every column under a single
    // Caesar shift, whose IC stays close to that expected language value.
    // Wrong lengths mix several shifts per column and move the IC away from it.
    static int FindLengthFriedman(const std::string& encoded,
                                  const std::vector<double>& language_freq) {
        int n_symbols = (int)language_freq.size();
        double expected_ic = 0;
        for (double probability : language_freq)
            expected_ic += probability * probability;

        std::vector<double> avg_ic(MAX_KEY_LENGTH + 1, 0);

        for (int k = 1; k <= MAX_KEY_LENGTH; ++k) {
            double ic_sum = 0;
            int columns_used = 0;

            for (int i = 0; i < k; ++i) {
                std::vector<long long> freq(n_symbols, 0);
                long long n = 0;
                for (int j = i; j < (int)encoded.size(); j += k) {
                    ++freq[encoded[j] - 'A'];
                    ++n;
                }
                if (n < 2) continue; // column too short to define an IC

                double ic = 0;
                for (int c = 0; c < n_symbols; ++c)
                    ic += (double)freq[c] * (freq[c] - 1);
                ic /= (double)n * (n - 1);

                ic_sum += ic;
                ++columns_used;
            }

            if (columns_used > 0) avg_ic[k] = ic_sum / columns_used;
        }

        double best_error = std::abs(avg_ic[1] - expected_ic);
        for (int k = 2; k <= MAX_KEY_LENGTH; ++k) {
            double error = std::abs(avg_ic[k] - expected_ic);
            best_error = std::min(best_error, error);
        }

        // Multiples of the real key length also create single-shift columns,
        // and their shorter columns may land closer to expected_ic by chance.
        // Return the smallest candidate whose error is near the best one.
        double tolerance = expected_ic * 0.1;
        for (int k = 1; k <= MAX_KEY_LENGTH; ++k)
            if (std::abs(avg_ic[k] - expected_ic) <= best_error + tolerance) return k;

        return 1;
    }

    // Frequency analysis: every i-th letter (mod key length) is a Caesar
    // cipher with the same shift. For each possible shift, compare the
    // resulting letter distribution with the language's letter frequencies
    // (chi-squared) and keep the shift that matches best.
    static std::string FrequentAnalysis(const std::string& encoded,
                                        int length,
                                        const std::vector<double>& language_freq) {
        int n_symbols = (int)language_freq.size();
        std::string actual_key(length, ' ');

        for (int i = 0; i < length; ++i) {
            std::vector<int> freq(n_symbols, 0);
            int n = 0;
            for (int j = i; j < (int)encoded.size(); j += length) {
                ++freq[encoded[j] - 'A'];
                ++n;
            }

            int best_shift = 0;
            double best_chi2 = -1;
            for (int shift = 0; shift < n_symbols; ++shift) {
                double chi2 = 0;
                for (int c = 0; c < n_symbols; ++c) {
                    double expected = language_freq[c] * n;
                    double observed = freq[(c + shift) % n_symbols];
                    if (expected > 0)
                        chi2 += (observed - expected) * (observed - expected) / expected;
                    else if (observed > 0)
                        chi2 += 1e9; // symbol never occurs in this language
                }
                if (best_chi2 < 0 || chi2 < best_chi2) {
                    best_chi2 = chi2;
                    best_shift = shift;
                }
            }
            actual_key[i] = (char)('A' + best_shift);
        }
        return actual_key;
    }

    // ----- public API ----------------------------------------------------

    VigenereCypher(const std::string& _key, Alphabet _alphabet) : alphabet(std::move(_alphabet)) {
        key = alphabet.ExtractLetters(_key);
        if (key.empty()) throw std::runtime_error("Key contains no alphabet symbols");
    }

    // Recovers the key from a ciphertext and returns a cypher ready to
    // decode it. The key length is estimated with both the Kasiski
    // examination and the Friedman test; when they disagree (mostly on
    // short texts), the key is derived for both lengths and the one whose
    // decryption contains more dictionary words wins. The alphabet must
    // therefore carry the language's resources, i.e. come from the
    // resource-aware factories.
    static VigenereCypher Crack(const std::string& encoded, const Alphabet& alphabet) {
        if (alphabet.Frequencies().empty())
            throw std::runtime_error("Crack needs an alphabet built with a word list");

        std::string letters = alphabet.ExtractLetters(encoded);
        const std::vector<double>& language_freq = alphabet.Frequencies();

        int len_kasiski = FindLengthKasiski(letters, alphabet.size());
        int len_friedman = FindLengthFriedman(letters, language_freq);

        std::string key_kasiski =
            alphabet.SymbolsToText(FrequentAnalysis(letters, len_kasiski, language_freq));
        if (len_kasiski == len_friedman) return VigenereCypher(key_kasiski, alphabet);

        std::string key_friedman =
            alphabet.SymbolsToText(FrequentAnalysis(letters, len_friedman, language_freq));

        VigenereCypher by_kasiski(key_kasiski, alphabet);
        VigenereCypher by_friedman(key_friedman, alphabet);

        long long words_kasiski = alphabet.CountDictionaryWords(by_kasiski.decode(encoded));
        long long words_friedman = alphabet.CountDictionaryWords(by_friedman.decode(encoded));

        return words_kasiski >= words_friedman ? by_kasiski : by_friedman;
    }

    std::string GetKey() const { return alphabet.SymbolsToText(key); }

    std::string encode(const std::string& message) const {
        std::string out;
        int i = 0, key_pos = 0;
        while (i < (int)message.size()) {
            int start = i;
            char c = alphabet.SymbolToChar(message, i);
            if (c == 0) {
                out.append(message, start, i - start);
            } else {
                out += alphabet.CharToSymbol(helper_increase(c, key[key_pos % key.size()]));
                ++key_pos;
            }
        }
        return out;
    }

    std::string encode(const std::string& message, const std::string& other_key) const {
        return VigenereCypher(other_key, alphabet).encode(message);
    }

    std::string decode(const std::string& message) const {
        std::string out;
        int i = 0, key_pos = 0;
        while (i < (int)message.size()) {
            int start = i;
            char c = alphabet.SymbolToChar(message, i);
            if (c == 0) {
                out.append(message, start, i - start);
            } else {
                out += alphabet.CharToSymbol(helper_decrease(c, key[key_pos % key.size()]));
                ++key_pos;
            }
        }
        return out;
    }
};
