#include "segmenter.hpp"
#include "vigenere.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    // usage: ./vigenere [en|de] [encrypted-file]
    std::string lang = argc > 1 ? argv[1] : "en";
    std::string input_file = argc > 2 ? argv[2] : "vigenere/data/long-encrypted.txt";

    std::string dict_file =
        lang == "de" ? "vigenere/wordlist-de_DE/de_DE.txt" : "vigenere/wordlist-en_US/en_US.txt";

    std::string input_encoded;
    std::ifstream get_input(input_file);
    if (!get_input) {
        throw std::runtime_error("Input file is not opened");
    }
    std::string line;
    while (std::getline(get_input, line))
        input_encoded += line + " ";

    std::ifstream wordList(dict_file);
    if (!wordList) {
        throw std::runtime_error("WordList is not opened");
    }
    std::string read;
    std::vector<std::string> activeWordList;
    while (wordList >> read) {
        activeWordList.push_back(read);
    }

    Alphabet alphabet =
        lang == "de" ? Alphabet::German(activeWordList) : Alphabet::English(activeWordList);

    VigenereCypher VC = VigenereCypher::Crack(input_encoded, alphabet);
    std::string decoded = VC.decode(input_encoded);

    std::cout << "1." << VC.GetKey() << "\n";
    std::cout << "2." << decoded << '\n';

    // If the plaintext has no real spaces (they were stripped before
    // encryption), recover the word boundaries with the dictionary. A run of
    // non-space characters longer than any real word signals spaceless text.
    size_t run = 0, longest_run = 0;
    for (char c : decoded) {
        if (c == ' ' || c == '\n' || c == '\t')
            run = 0;
        else
            longest_run = std::max(longest_run, ++run);
    }
    if (longest_run > 40) {
        std::cout << "3." << WordSegmenter::Segment(alphabet, decoded) << '\n';
    }

    return 0;
}
