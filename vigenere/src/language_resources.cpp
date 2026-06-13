#include "language_resources.hpp"

#include "alphabet.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>

LanguageResources::LanguageResources(const Alphabet& alphabet,
                                     const std::vector<std::string>& words,
                                     const std::string& cache_file,
                                     std::string unigram_file)
    : dictionary_(words.begin(), words.end()), unigram_file_(std::move(unigram_file)) {
    if (dictionary_.empty()) {
        throw std::runtime_error("Language resources need a non-empty dictionary");
    }
    for (const std::string& word : dictionary_) {
        longest_word_ = std::max(longest_word_, word.size());
    }
    load_frequencies(alphabet, cache_file, words);
}

std::vector<double>
LanguageResources::calculate_frequencies(const Alphabet& alphabet,
                                         const std::vector<std::string>& words) {
    std::vector<long long> count(alphabet.size(), 0);
    long long total = 0;
    for (const std::string& word : words) {
        std::size_t i = 0;
        while (i < word.size()) {
            char symbol = alphabet.symbol_to_char(word, i);
            if (symbol != 0) {
                ++count[symbol - 'A'];
                ++total;
            }
        }
    }
    if (total == 0) {
        throw std::runtime_error("Cannot calculate frequencies without alphabet symbols");
    }

    std::vector<double> frequencies(alphabet.size());
    for (std::size_t i = 0; i < frequencies.size(); ++i) {
        frequencies[i] = static_cast<double>(count[i]) / static_cast<double>(total);
    }
    return frequencies;
}

long long LanguageResources::score_dictionary_words(const std::string& text,
                                                    const Alphabet& alphabet) const {
    long long score = 0;
    std::size_t longest_run = 0;
    std::string word;
    std::size_t i = 0;
    while (i < text.size()) {
        char symbol = alphabet.symbol_to_char(text, i);
        if (symbol != 0) {
            word += alphabet.char_to_symbol(symbol);
            longest_run = std::max(longest_run, word.size());
        } else {
            if (!word.empty() && dictionary_.contains(word)) { ++score; }
            word.clear();
        }
    }
    if (!word.empty() && dictionary_.contains(word)) { ++score; }

    if (score > 0 || longest_run <= longest_word_) { return score; }

    i = 0;
    while (i < text.size()) {
        std::size_t remaining = text.size() - i;
        std::size_t length = longest_word_ < remaining ? longest_word_ : remaining;
        while (length > 0 && !dictionary_.contains(text.substr(i, length))) {
            --length;
        }
        if (length > 0) {
            score += static_cast<long long>(length * length);
            i += length;
        } else {
            alphabet.symbol_to_char(text, i);
        }
    }
    return score;
}

void LanguageResources::load_frequencies(const Alphabet& alphabet,
                                         const std::string& cache_file,
                                         const std::vector<std::string>& words) {
    std::ifstream input(cache_file);
    std::vector<double> cached(alphabet.size());
    std::size_t loaded = 0;
    double value = 0;
    double sum = 0;
    bool entries_valid = true;
    while (loaded < cached.size() && input >> value) {
        if (!std::isfinite(value) || value < 0) {
            entries_valid = false;
            break;
        }
        cached[loaded++] = value;
        sum += value;
    }

    bool valid =
        entries_valid && loaded == cached.size() && sum > 0.99 && sum < 1.01 && !(input >> value);
    if (valid) {
        frequencies_ = std::move(cached);
        return;
    }

    frequencies_ = calculate_frequencies(alphabet, words);
    std::ofstream output(cache_file);
    if (!output) { throw std::runtime_error("Cannot write frequency cache: " + cache_file); }
    output.precision(12);
    for (double frequency : frequencies_) {
        output << frequency << "\n";
    }
    if (!output) {
        throw std::runtime_error("Failed while writing frequency cache: " + cache_file);
    }
}
