#pragma once

#include "alphabet.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

class VigenereTestAccess;
class VigenereBenchmarkAccess;

class VigenereCipher {
  private:
    Alphabet alphabet;
    std::string key; // internal chars, one per key symbol
    static constexpr std::size_t max_key_length = 10;

    friend class VigenereTestAccess;
    friend class VigenereBenchmarkAccess;

    [[nodiscard]] char increase_symbol(char a, char b) const {
        return static_cast<char>(((a - 'A' + b - 'A') % static_cast<int>(alphabet.size())) + 'A');
    }
    [[nodiscard]] char decrease_symbol(char a, char b) const {
        return static_cast<char>(
            ((a - b + static_cast<int>(alphabet.size())) % static_cast<int>(alphabet.size())) +
            'A');
    }

    static std::size_t find_length_kasiski(const std::string& encoded, std::size_t n_symbols) {
        std::vector<std::size_t> distances;
        std::vector<std::size_t> last_pos(n_symbols * n_symbols * n_symbols, encoded.size());

        for (std::size_t i = 0; i + 3 <= encoded.size(); ++i) {
            std::size_t code =
                ((((encoded[i] - 'A') * n_symbols) + (encoded[i + 1] - 'A')) * n_symbols) +
                (encoded[i + 2] - 'A');
            if (last_pos[code] != encoded.size()) { distances.push_back(i - last_pos[code]); }
            last_pos[code] = i;
        }

        if (distances.empty()) { return 1; }

        // Multiplication by k normalizes the 1/k chance of random divisibility.
        std::vector<long long> score(max_key_length + 1, 0);
        for (std::size_t k = 1; k <= max_key_length; ++k) {
            long long count = 0;
            for (std::size_t d : distances) {
                if (d % k == 0) { ++count; }
            }
            score[k] = count * static_cast<long long>(k);
        }

        long long best_score = 0;
        for (std::size_t k = 1; k <= max_key_length; ++k) {
            best_score = std::max(best_score, score[k]);
        }

        for (std::size_t k = 1; k <= max_key_length; ++k) {
            if (score[k] * 10 >= best_score * 9) { return k; }
        }

        return 1;
    }

    // The correct key length produces columns whose index of coincidence is
    // close to the language's expected value, sum(p_c^2).
    static std::size_t find_length_friedman(const std::string& encoded,
                                            const std::vector<double>& language_freq) {
        std::size_t n_symbols = language_freq.size();
        double expected_ic = 0;
        for (double probability : language_freq) {
            expected_ic += probability * probability;
        }

        std::vector<double> avg_ic(max_key_length + 1, 0);

        for (std::size_t k = 1; k <= max_key_length; ++k) {
            double ic_sum = 0;
            std::size_t columns_used = 0;

            for (std::size_t i = 0; i < k; ++i) {
                std::vector<long long> freq(n_symbols, 0);
                long long n = 0;
                for (std::size_t j = i; j < encoded.size(); j += k) {
                    ++freq[encoded[j] - 'A'];
                    ++n;
                }
                if (n < 2) { continue; } // column too short to define an IC

                double ic = 0;
                for (std::size_t c = 0; c < n_symbols; ++c) {
                    ic += static_cast<double>(freq[c]) * static_cast<double>(freq[c] - 1);
                }
                ic /= static_cast<double>(n) * static_cast<double>(n - 1);

                ic_sum += ic;
                ++columns_used;
            }

            if (columns_used > 0) { avg_ic[k] = ic_sum / static_cast<double>(columns_used); }
        }

        double best_error = std::abs(avg_ic[1] - expected_ic);
        for (std::size_t k = 2; k <= max_key_length; ++k) {
            double error = std::abs(avg_ic[k] - expected_ic);
            best_error = std::min(best_error, error);
        }

        // Prefer the smallest near-best candidate to avoid key-length multiples.
        double tolerance = expected_ic * 0.1;
        for (std::size_t k = 1; k <= max_key_length; ++k) {
            if (std::abs(avg_ic[k] - expected_ic) <= best_error + tolerance) { return k; }
        }

        return 1;
    }

    // Each key column is a Caesar cipher; choose its lowest chi-squared shift.
    static std::string frequency_analysis(const std::string& encoded,
                                          std::size_t length,
                                          const std::vector<double>& language_freq) {
        std::size_t n_symbols = language_freq.size();
        std::string actual_key(length, ' ');

        for (std::size_t i = 0; i < length; ++i) {
            std::vector<int> freq(n_symbols, 0);
            std::size_t n = 0;
            for (std::size_t j = i; j < encoded.size(); j += length) {
                ++freq[encoded[j] - 'A'];
                ++n;
            }

            std::size_t best_shift = 0;
            double best_chi2 = -1;
            for (std::size_t shift = 0; shift < n_symbols; ++shift) {
                double chi2 = 0;
                for (std::size_t c = 0; c < n_symbols; ++c) {
                    double expected = language_freq[c] * static_cast<double>(n);
                    double observed = freq[(c + shift) % n_symbols];
                    if (expected > 0) {
                        chi2 += (observed - expected) * (observed - expected) / expected;
                    } else if (observed > 0) {
                        chi2 += 1e9; // symbol never occurs in this language
                    }
                }
                if (best_chi2 < 0 || chi2 < best_chi2) {
                    best_chi2 = chi2;
                    best_shift = shift;
                }
            }
            actual_key[i] = static_cast<char>('A' + best_shift);
        }
        return actual_key;
    }

  public:
    VigenereCipher(const std::string& key_text, Alphabet selected_alphabet)
        : alphabet(std::move(selected_alphabet)) {
        key = alphabet.extract_letters(key_text);
        if (key.empty()) { throw std::runtime_error("Key contains no alphabet symbols"); }
    }

    // When the length estimators disagree, prefer the candidate whose
    // decryption has the stronger dictionary score.
    static VigenereCipher crack(const std::string& encoded, const Alphabet& alphabet) {
        if (alphabet.resources().frequencies().empty()) {
            throw std::runtime_error("crack needs an alphabet built with a word list");
        }

        std::string letters = alphabet.extract_letters(encoded);
        if (letters.empty()) {
            throw std::invalid_argument("Ciphertext contains no alphabet symbols");
        }
        const std::vector<double>& language_freq = alphabet.resources().frequencies();

        std::size_t len_kasiski = find_length_kasiski(letters, alphabet.size());
        std::size_t len_friedman = find_length_friedman(letters, language_freq);

        std::string key_kasiski =
            alphabet.symbols_to_text(frequency_analysis(letters, len_kasiski, language_freq));
        if (len_kasiski == len_friedman) { return VigenereCipher(key_kasiski, alphabet); }

        std::string key_friedman =
            alphabet.symbols_to_text(frequency_analysis(letters, len_friedman, language_freq));

        VigenereCipher by_kasiski(key_kasiski, alphabet);
        VigenereCipher by_friedman(key_friedman, alphabet);

        long long words_kasiski =
            alphabet.resources().score_dictionary_words(by_kasiski.decode(encoded), alphabet);
        long long words_friedman =
            alphabet.resources().score_dictionary_words(by_friedman.decode(encoded), alphabet);

        return words_kasiski >= words_friedman ? by_kasiski : by_friedman;
    }

    [[nodiscard]] std::string get_key() const { return alphabet.symbols_to_text(key); }

    [[nodiscard]] std::string encode(const std::string& message) const {
        std::string out;
        std::size_t i = 0;
        std::size_t key_pos = 0;
        while (i < message.size()) {
            std::size_t start = i;
            char c = alphabet.symbol_to_char(message, i);
            if (c == 0) {
                out.append(message, start, i - start);
            } else {
                out += alphabet.char_to_symbol(increase_symbol(c, key[key_pos % key.size()]));
                ++key_pos;
            }
        }
        return out;
    }

    [[nodiscard]] std::string encode(const std::string& message,
                                     const std::string& other_key) const {
        return VigenereCipher(other_key, alphabet).encode(message);
    }

    [[nodiscard]] std::string decode(const std::string& message) const {
        std::string out;
        std::size_t i = 0;
        std::size_t key_pos = 0;
        while (i < message.size()) {
            std::size_t start = i;
            char c = alphabet.symbol_to_char(message, i);
            if (c == 0) {
                out.append(message, start, i - start);
            } else {
                out += alphabet.char_to_symbol(decrease_symbol(c, key[key_pos % key.size()]));
                ++key_pos;
            }
        }
        return out;
    }
};
