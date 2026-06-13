#pragma once

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

inline std::string read_text_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) { throw std::runtime_error("Cannot open text file: " + path); }

    std::string text(std::istreambuf_iterator<char>(input), {});
    if (input.bad()) { throw std::runtime_error("Failed while reading text file: " + path); }
    return text;
}

inline std::vector<std::string> read_word_list(const std::string& path) {
    std::ifstream input(path);
    if (!input) { throw std::runtime_error("Cannot open word list: " + path); }

    std::vector<std::string> words;
    std::string word;
    while (input >> word) {
        words.push_back(word);
    }
    if (input.bad()) { throw std::runtime_error("Failed while reading word list: " + path); }
    return words;
}
