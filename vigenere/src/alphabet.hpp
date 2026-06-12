#pragma once

#include <fstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

// The set of symbols the cipher operates on, plus the language's resources
// (letter distribution and dictionary). The first 26 symbols are always
// A-Z; a language may add extra symbols (the German umlauts and eszett),
// which are multi-byte UTF-8 sequences in the text. Internally every symbol
// is represented as the single char 'A' + index, so the rest of the cipher
// can keep working on plain std::string.
class Alphabet {
  private:
    // extra symbols beyond A-Z as UTF-8 strings;
    // the symbol of extras[i] is 'A' + 26 + i
    std::vector<std::string> extras;

    // the language's letter distribution, loaded during resource-aware construction
    std::vector<double> frequencies;

    // the language's words, stored during resource-aware construction
    std::unordered_set<std::string> dictionary;

    // byte length of the longest dictionary word; a longer symbol run in a
    // text signals that its separators were stripped before encryption
    size_t longest_word = 0;

    // path of the language's unigram frequency list ("word<TAB>count"), set
    // during resource-aware construction; empty for a resource-less alphabet
    std::string unigram_file;

    Alphabet() = default;

    Alphabet(std::vector<std::string> extra_symbols,
             const std::string& cache_file,
             const std::vector<std::string>& words,
             std::string unigram)
        : extras(std::move(extra_symbols)), dictionary(words.begin(), words.end()),
          unigram_file(std::move(unigram)) {
        for (const std::string& word : dictionary)
            if (word.size() > longest_word) longest_word = word.size();
        LoadFrequencies(cache_file, words);
    }

  public:
    int size() const { return 26 + (int)extras.size(); }

    // Reads the symbol starting at byte position i and advances i past it.
    // Returns the internal char, or 0 (advancing one byte) if no alphabet
    // symbol starts there.
    char SymbolToChar(const std::string& text, int& i) const {
        char c = text[i];
        if (c >= 'A' && c <= 'Z') {
            ++i;
            return c;
        }
        for (int e = 0; e < (int)extras.size(); ++e) {
            const auto& character = extras[e];
            if (text.compare(i, character.size(), character) == 0) {
                i += character.size();
                return (char)('A' + 26 + e);
            }
        }
        ++i;
        return 0;
    }

    std::string CharToSymbol(char ch) const {
        if (ch < 'A' + 26) return std::string(1, ch);
        return extras[ch - 'A' - 26];
    }

    std::string SymbolsToText(const std::string& internal_symbols) const {
        std::string text;
        for (char symbol : internal_symbols)
            text += CharToSymbol(symbol);
        return text;
    }

    static Alphabet English() { return Alphabet{}; }

    static Alphabet
    English(const std::vector<std::string>& words,
            const std::string& cache_file = "vigenere/wordlist-en_US/frequencies.txt",
            const std::string& unigram_file = "vigenere/wordlist-en_US/unigram_freq.txt") {
        return Alphabet({}, cache_file, words, unigram_file);
    }

    static Alphabet German() {
        Alphabet a;
        a.extras = {"Ä", "Ö", "Ü", "ß"};
        return a;
    }

    static Alphabet
    German(const std::vector<std::string>& words,
           const std::string& cache_file = "vigenere/wordlist-de_DE/frequencies.txt",
           const std::string& unigram_file = "vigenere/wordlist-de_DE/unigram_freq.txt") {
        return Alphabet({"Ä", "Ö", "Ü", "ß"}, cache_file, words, unigram_file);
    }

    // Converts text to the internal one-char-per-symbol representation,
    // dropping everything that is not an alphabet symbol.
    std::string ExtractLetters(const std::string& text) const {
        std::string letters;
        int i = 0;
        while (i < (int)text.size()) {
            char c = SymbolToChar(text, i);
            if (c != 0) letters += c;
        }
        return letters;
    }

    // Builds the reference letter distribution of the language by counting
    // the symbols of every word in the dictionary.
    std::vector<double> LetterFrequencies(const std::vector<std::string>& words) const {
        std::vector<long long> count(size(), 0);
        long long total = 0;
        for (const std::string& word : words) {
            int i = 0;
            while (i < (int)word.size()) {
                char c = SymbolToChar(word, i);
                if (c != 0) {
                    ++count[c - 'A'];
                    ++total;
                }
            }
        }

        std::vector<double> freq(size());
        for (int i = 0; i < size(); ++i)
            freq[i] = (double)count[i] / total;
        return freq;
    }

    const std::vector<double>& Frequencies() const { return frequencies; }

    const std::unordered_set<std::string>& Dictionary() const { return dictionary; }

    const std::string& UnigramFile() const { return unigram_file; }

    // Counts how many of the text's words appear in the dictionary —
    // a measure of how "readable" a candidate decryption is.
    long long CountDictionaryWords(const std::string& text) const {
        long long count = 0;
        size_t longest_run = 0;
        std::string word;
        int i = 0;
        while (i < (int)text.size()) {
            char c = SymbolToChar(text, i);
            if (c != 0) {
                word += CharToSymbol(c);
                if (word.size() > longest_run) longest_run = word.size();
            } else {
                if (!word.empty() && dictionary.count(word)) ++count;
                word.clear();
            }
        }
        if (!word.empty() && dictionary.count(word)) ++count;

        // The separator-based count above is only wrong when the separators
        // were stripped before encryption: it then sees one giant "word". A
        // run no real word can explain, with nothing counted, signals that
        // case; fall back to greedy longest-match inside the runs. Matches
        // score length^2 instead of 1 because mostly-garbled text still
        // fragments into many 1-2 letter words, which a flat count would
        // reward over the fewer-but-longer words of a correct decryption.
        if (count > 0 || longest_run <= longest_word) return count;

        i = 0;
        while (i < (int)text.size()) {
            size_t remaining = text.size() - i;
            size_t len = longest_word < remaining ? longest_word : remaining;
            while (len > 0 && !dictionary.count(text.substr(i, len)))
                --len;
            if (len > 0) {
                count += (long long)(len * len);
                i += (int)len;
            } else {
                SymbolToChar(text, i); // no word starts here; skip one symbol
            }
        }
        return count;
    }

  private:
    // Fills the frequencies member from the cache file if it is valid for
    // this alphabet (one value per symbol, summing to ~1). Otherwise falls
    // back to counting the dictionary words — the only O(dictionary) path —
    // and rewrites the cache for the next run.
    void LoadFrequencies(const std::string& cache_file, const std::vector<std::string>& words) {
        std::ifstream in(cache_file);
        std::vector<double> cached(size());
        int loaded = 0;
        double value, sum = 0;
        while (loaded < size() && in >> value) {
            cached[loaded++] = value;
            sum += value;
        }

        bool valid = loaded == size() && sum > 0.99 && sum < 1.01 &&
                     !(in >> value); // extra values = file for another alphabet
        if (valid) {
            frequencies = cached;
            return;
        }

        frequencies = LetterFrequencies(words);
        std::ofstream out(cache_file);
        out.precision(12);
        for (double f : frequencies)
            out << f << "\n";
    }
};
