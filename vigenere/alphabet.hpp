#pragma once

#include <string>
#include <fstream>
#include <vector>

// The set of symbols the cipher operates on. The first 26 symbols are always
// A-Z; a language may add extra symbols (the German umlauts and eszett),
// which are multi-byte UTF-8 sequences in the text. Internally every symbol
// is represented as the single char 'A' + index, so the rest of the cipher
// can keep working on plain std::string.
class Alphabet{
private:
    // extra symbols beyond A-Z as UTF-8 strings;
    // the symbol of extras[i] is 'A' + 26 + i
    std::vector<std::string> extras;

    // the language's letter distribution, filled by LoadFrequencies
    std::vector<double> frequencies;

public:
    int size() const {
        return 26 + (int)extras.size();
    }

    // Reads the symbol starting at byte position i and advances i past it.
    // Returns the internal char, or 0 (advancing one byte) if no alphabet
    // symbol starts there.
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

    // Converts text to the internal one-char-per-symbol representation,
    // dropping everything that is not an alphabet symbol.
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
