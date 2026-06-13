#pragma once

#include <string>
#include <unordered_set>
#include <vector>

class Alphabet;

class LanguageResources {
  private:
    std::vector<double> frequencies_;
    std::unordered_set<std::string> dictionary_;
    std::size_t longest_word_ = 0;
    std::string unigram_file_;

    void load_frequencies(const Alphabet& alphabet,
                          const std::string& cache_file,
                          const std::vector<std::string>& words);

  public:
    LanguageResources() = default;
    LanguageResources(const Alphabet& alphabet,
                      const std::vector<std::string>& words,
                      const std::string& cache_file,
                      std::string unigram_file);

    static std::vector<double> calculate_frequencies(const Alphabet& alphabet,
                                                     const std::vector<std::string>& words);

    [[nodiscard]] const std::vector<double>& frequencies() const { return frequencies_; }
    [[nodiscard]] const std::unordered_set<std::string>& dictionary() const { return dictionary_; }
    [[nodiscard]] const std::string& unigram_file() const { return unigram_file_; }

    [[nodiscard]] long long score_dictionary_words(const std::string& text,
                                                   const Alphabet& alphabet) const;
};
