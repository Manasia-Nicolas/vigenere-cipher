#pragma once

#include "language_resources.hpp"

#include <concepts>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Converts between a language's UTF-8 symbols and the one-char-per-symbol
// representation used by the cipher. Language dictionaries, frequencies, and
// scoring are composed through LanguageResources.
class Alphabet {
  private:
    std::vector<std::string> extras_;
    LanguageResources resources_;

    explicit Alphabet(std::vector<std::string> extra_symbols) : extras_(std::move(extra_symbols)) {}

    Alphabet(std::vector<std::string> extra_symbols,
             const std::string& cache_file,
             const std::vector<std::string>& words,
             std::string unigram_file)
        : extras_(std::move(extra_symbols)),
          resources_(*this, words, cache_file, std::move(unigram_file)) {}

  public:
    Alphabet() = default;

    [[nodiscard]] std::size_t size() const { return 26 + extras_.size(); }

    char symbol_to_char(const std::string& text, std::size_t& i) const {
        if (i >= text.size()) { throw std::out_of_range("symbol position is outside the text"); }

        char character = text[i];
        if (character >= 'A' && character <= 'Z') {
            ++i;
            return character;
        }
        for (std::size_t extra = 0; extra < extras_.size(); ++extra) {
            const std::string& symbol = extras_[extra];
            if (text.compare(i, symbol.size(), symbol) == 0) {
                i += symbol.size();
                return static_cast<char>('A' + 26 + extra);
            }
        }
        ++i;
        return 0;
    }

    template <std::integral Index>
        requires(!std::same_as<std::remove_cv_t<Index>, bool> && !std::is_const_v<Index>)
    char symbol_to_char(const std::string& text, Index& i) const {
        if constexpr (std::is_signed_v<Index>) {
            if (i < 0) { throw std::out_of_range("symbol position cannot be negative"); }
        }

        std::size_t position = static_cast<std::size_t>(i);
        char symbol = symbol_to_char(text, position);
        if (!std::in_range<Index>(position)) {
            throw std::out_of_range("symbol position does not fit the caller's index type");
        }
        i = static_cast<Index>(position);
        return symbol;
    }

    [[nodiscard]] std::string char_to_symbol(char character) const {
        int index = character - 'A';
        if (index < 0 || static_cast<std::size_t>(index) >= size()) {
            throw std::out_of_range("character is not an internal alphabet symbol");
        }
        if (index < 26) { return std::string(1, character); }
        return extras_[static_cast<std::size_t>(index - 26)];
    }

    [[nodiscard]] std::string symbols_to_text(const std::string& internal_symbols) const {
        std::string text;
        for (char symbol : internal_symbols) {
            text += char_to_symbol(symbol);
        }
        return text;
    }

    [[nodiscard]] std::string extract_letters(const std::string& text) const {
        std::string letters;
        std::size_t i = 0;
        while (i < text.size()) {
            char symbol = symbol_to_char(text, i);
            if (symbol != 0) { letters += symbol; }
        }
        return letters;
    }

    [[nodiscard]] const LanguageResources& resources() const { return resources_; }

    static Alphabet english() { return Alphabet{}; }

    static Alphabet
    english(const std::vector<std::string>& words,
            const std::string& cache_file = "vigenere/wordlist-en_US/frequencies.txt",
            const std::string& unigram_file = "vigenere/wordlist-en_US/unigram_freq.txt") {
        return Alphabet({}, cache_file, words, unigram_file);
    }

    static Alphabet german() { return Alphabet({"Ä", "Ö", "Ü", "ß"}); }

    static Alphabet
    german(const std::vector<std::string>& words,
           const std::string& cache_file = "vigenere/wordlist-de_DE/frequencies.txt",
           const std::string& unigram_file = "vigenere/wordlist-de_DE/unigram_freq.txt") {
        return Alphabet({"Ä", "Ö", "Ü", "ß"}, cache_file, words, unigram_file);
    }
};
