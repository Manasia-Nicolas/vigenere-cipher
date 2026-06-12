#pragma once

#include "alphabet.hpp"

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <cmath>
#include <algorithm>

// Re-inserts word boundaries into text whose spaces were stripped before
// encryption. After decryption you hold a run of letters like
// "THEVIGENERECIPHERISAMETHODOF..." — correct plaintext, but unreadable.
// The segmenter splits it back into "THE VIGENERE CIPHER IS A METHOD OF ...".
//
// Each candidate word has a COST (lower = more likely); the best segmentation
// is the one of minimum total cost, found by a dynamic program over the
// symbols of the text. Two cost models are supported:
//
//   * Unigram model (Build with a "word<TAB>count" frequency file): the cost
//     of a word is -log10(count / N) — common words are cheap, rare words
//     expensive. This is the model that gets "A LONG WAY OFF" right where a
//     length-only heuristic greedily merges them into "ALONG WAY OFF".
//
//   * Dictionary-only fallback (no frequency file, e.g. for German): a word's
//     cost is -length^2, which rewards covering the text with long real words.
//
// In both models a symbol that is part of no known word is kept as a costly
// one-symbol token, so a segmentation always exists (proper nouns survive).
class WordSegmenter{
private:
    static constexpr int MAX_WORD_LENGTH = 30;

    std::unordered_map<std::string, double> word_cost;  // known word -> cost
    double unknown_cost = 1.0;                           // cost of one unknown symbol

    // Uppercases an ASCII word and returns false if it holds any non-letter
    // (apostrophes, digits) so we keep only clean A-Z keys.
    static bool NormalizeAscii(std::string& w){
        for(char& c : w){
            if(c >= 'a' && c <= 'z') c = c - 'a' + 'A';
            else if(c < 'A' || c > 'Z') return false;
        }
        return !w.empty();
    }

public:
    // Unigram cost model from a Norvig-style frequency file, with the
    // dictionary supplying rare-but-valid words the frequency list may miss.
    // If the frequency file cannot be opened, falls back to the
    // dictionary-only length model so the segmenter still works (e.g. German).
    static WordSegmenter Build(const std::string& frequency_file,
                               const std::unordered_set<std::string>& dictionary){
        WordSegmenter s;

        std::ifstream in(frequency_file);
        double total = 0;
        std::vector<std::pair<std::string, double>> entries;
        std::string word;
        double count;
        while(in >> word >> count){
            if(NormalizeAscii(word) && count > 0){
                entries.emplace_back(word, count);
                total += count;
            }
        }

        if(total > 0){
            double log_total = std::log10(total);
            for(const auto& [w, c] : entries){
                double cost = log_total - std::log10(c);
                auto it = s.word_cost.find(w);
                if(it == s.word_cost.end() || cost < it->second)
                    s.word_cost[w] = cost;
            }
            // dictionary words missing from the frequency list are valid but
            // rare: give them the floor cost of a least-frequent unigram.
            for(const std::string& w : dictionary)
                s.word_cost.emplace(w, log_total);
            // an unknown single symbol is costlier than any real word, so it
            // is only used when nothing else covers the position
            s.unknown_cost = log_total + 10;
        } else {
            // dictionary-only fallback: reward length (cost = -length^2)
            for(const std::string& w : dictionary){
                int len = 0, i = 0;
                while(i < (int)w.size()){           // count symbols, not bytes
                    // any byte that is a UTF-8 continuation (10xxxxxx) is skipped
                    if((w[i] & 0xC0) != 0x80) ++len;
                    ++i;
                }
                s.word_cost[w] = -(double)len * len;
            }
            s.unknown_cost = 1.0;
        }
        return s;
    }

    std::string Segment(const std::string& text, const Alphabet& alphabet) const {
        // Tokenize into symbols (each is its UTF-8 text form), so words are
        // concatenated in symbol units, not bytes.
        std::vector<std::string> symbols;
        int i = 0;
        while(i < (int)text.size()){
            char c = alphabet.SymbolToChar(text, i);
            if(c != 0)
                symbols.push_back(alphabet.CharToSymbol(c));
        }

        int n = (int)symbols.size();
        const double INF = 1e18;
        std::vector<double> best(n + 1, INF);
        std::vector<int> prev(n + 1, -1);  // start index of the word ending at i
        best[0] = 0;

        for(int end = 1; end <= n; ++end){
            std::string word;
            int lo = std::max(0, end - MAX_WORD_LENGTH);
            for(int start = end - 1; start >= lo; --start){
                word = symbols[start] + word;   // grow the word leftwards
                if(best[start] >= INF)
                    continue;

                auto it = word_cost.find(word);
                double cost;
                if(it != word_cost.end())
                    cost = it->second;
                else if(end - start == 1)
                    cost = unknown_cost;        // keep a lone unknown symbol
                else
                    continue;                   // no multi-symbol non-words

                double score = best[start] + cost;
                if(score < best[end]){
                    best[end] = score;
                    prev[end] = start;
                }
            }
        }

        // Reconstruct the words by following prev[] back from n.
        std::vector<std::string> out_words;
        for(int end = n; end > 0; end = prev[end]){
            std::string word;
            for(int k = prev[end]; k < end; ++k)
                word += symbols[k];
            out_words.push_back(word);
        }

        std::string result;
        for(int w = (int)out_words.size() - 1; w >= 0; --w){
            result += out_words[w];
            if(w > 0)
                result += ' ';
        }
        return result;
    }
};
