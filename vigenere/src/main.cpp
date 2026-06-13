#include "file_io.hpp"
#include "segmenter.hpp"
#include "vigenere.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    try {
        if (argc > 3) { throw std::invalid_argument("Usage: vigenere [en|de] [encrypted-file]"); }
        std::string lang = argc > 1 ? argv[1] : "en";
        if (lang != "en" && lang != "de") {
            throw std::invalid_argument("Unsupported language '" + lang + "'. Use 'en' or 'de'.");
        }
        std::string input_file = argc > 2 ? argv[2] : "vigenere/data/long-encrypted.txt";

        std::string dict_file = lang == "de" ? "vigenere/wordlist-de_DE/de_DE.txt"
                                             : "vigenere/wordlist-en_US/en_US.txt";

        std::string input_encoded = read_text_file(input_file);
        std::vector<std::string> active_word_list = read_word_list(dict_file);

        Alphabet alphabet =
            lang == "de" ? Alphabet::german(active_word_list) : Alphabet::english(active_word_list);

        VigenereCipher cipher = VigenereCipher::crack(input_encoded, alphabet);
        std::string decoded = cipher.decode(input_encoded);

        std::cout << "1." << cipher.get_key() << "\n";
        std::cout << "2." << decoded << '\n';

        std::string segmentation_input = decoded;
        while (!segmentation_input.empty() &&
               (segmentation_input.back() == ' ' || segmentation_input.back() == '\n' ||
                segmentation_input.back() == '\r' || segmentation_input.back() == '\t')) {
            segmentation_input.pop_back();
        }
        std::string letters = alphabet.extract_letters(segmentation_input);
        bool spaceless =
            !letters.empty() && segmentation_input == alphabet.symbols_to_text(letters);
        if (spaceless) {
            std::cout << "3." << WordSegmenter::segment(alphabet, segmentation_input) << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
