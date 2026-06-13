#pragma once

#include "alphabet.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Restores word boundaries by finding the minimum-cost segmentation.
// Unigram data gives words -log10(probability) costs; the dictionary-only
// fallback rewards long words with -length^2.
class WordSegmenter {
  private:
    std::unordered_map<std::string, double> word_cost; // known word -> cost
    double unknown_cost = 1.0;                         // cost of one unknown symbol
    std::size_t max_word_length = 1;                   // longest known word, in symbols

    // UTF-8 bytes pass unchanged; ASCII entries must contain only letters.
    static bool normalize_ascii(std::string& w) {
        for (char& c : w) {
            if (c >= 'a' && c <= 'z') {
                c = static_cast<char>(c - 'a' + 'A');
            } else if ((c & 0x80) == 0 && (c < 'A' || c > 'Z')) {
                return false;
            }
        }
        return !w.empty();
    }

    static std::size_t symbol_length(const std::string& word) {
        std::size_t length = 0;
        for (unsigned char byte : word) {
            if ((byte & 0xC0) != 0x80) { ++length; } // skip UTF-8 continuation bytes
        }
        return length;
    }

  public:
    static WordSegmenter build(const std::string& frequency_file,
                               const std::unordered_set<std::string>& dictionary) {
        if (dictionary.empty()) {
            throw std::invalid_argument("WordSegmenter needs a non-empty dictionary");
        }
        WordSegmenter s;

        std::ifstream in(frequency_file);
        double total = 0;
        std::vector<std::pair<std::string, double>> entries;
        std::string word;
        double count = 0;
        while (in >> word >> count) {
            if (normalize_ascii(word) && std::isfinite(count) && count > 0) {
                entries.emplace_back(word, count);
                total += count;
            }
        }

        if (std::isfinite(total) && total > 0) {
            double log_total = std::log10(total);
            for (const auto& [w, c] : entries) {
                double cost = log_total - std::log10(c);
                auto it = s.word_cost.find(w);
                if (it == s.word_cost.end() || cost < it->second) { s.word_cost[w] = cost; }
                s.max_word_length = std::max(s.max_word_length, symbol_length(w));
            }
            // Treat dictionary words missing from the frequency list as rare.
            for (const std::string& w : dictionary) {
                s.word_cost.emplace(w, log_total);
                s.max_word_length = std::max(s.max_word_length, symbol_length(w));
            }
            s.unknown_cost = log_total + 10;
        } else {
            for (const std::string& w : dictionary) {
                std::size_t len = symbol_length(w);
                double length = static_cast<double>(len);
                s.word_cost[w] = -length * length;
                s.max_word_length = std::max(s.max_word_length, len);
            }
            s.unknown_cost = 1.0;
        }
        return s;
    }

    [[nodiscard]] static std::string segment(const Alphabet& alphabet, const std::string& text) {
        return build(alphabet.resources().unigram_file(), alphabet.resources().dictionary())
            .segment(text, alphabet);
    }

    [[nodiscard]] std::string segment(const std::string& text, const Alphabet& alphabet) const {
        // Tokenize first so UTF-8 symbols count as one position.
        std::vector<std::string> symbols;
        std::size_t i = 0;
        while (i < text.size()) {
            char c = alphabet.symbol_to_char(text, i);
            if (c != 0) { symbols.push_back(alphabet.char_to_symbol(c)); }
        }

        std::size_t n = symbols.size();
        const double INF = 1e18;
        std::vector<double> best(n + 1, INF);
        std::vector<std::size_t> prev(n + 1); // start index of the word ending at i
        best[0] = 0;

        for (std::size_t end = 1; end <= n; ++end) {
            std::string word;
            std::size_t lo = end > max_word_length ? end - max_word_length : 0;
            for (std::size_t start = end; start-- > lo;) {
                word = symbols[start] + word; // grow the word leftwards
                if (best[start] >= INF) { continue; }

                auto it = word_cost.find(word);
                double cost = unknown_cost;
                if (it != word_cost.end()) {
                    cost = it->second;
                } else if (end - start != 1) {
                    continue; // no multi-symbol non-words
                }

                double score = best[start] + cost;
                if (score < best[end]) {
                    best[end] = score;
                    prev[end] = start;
                }
            }
        }

        std::vector<std::string> out_words;
        for (std::size_t end = n; end > 0; end = prev[end]) {
            std::string word;
            for (std::size_t k = prev[end]; k < end; ++k) {
                word += symbols[k];
            }
            out_words.push_back(word);
        }

        std::string result;
        for (auto word = out_words.rbegin(); word != out_words.rend(); ++word) {
            if (!result.empty()) { result += ' '; }
            result += *word;
        }
        return result;
    }
};
