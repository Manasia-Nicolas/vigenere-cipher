#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "../vigenere.hpp"
#include "../segmenter.hpp"

#include <cstdio>

// ---- helpers ---------------------------------------------------------------
// Tests run with the repository root as working directory (set in CMakeLists),
// so data files use the same relative paths as the main program.

static std::string ReadFile(const std::string& path){
    std::ifstream in(path);
    REQUIRE_MESSAGE(in, "cannot open ", path, " (tests must run from the repo root)");
    std::string text, line;
    while(std::getline(in, line))
        text += line + " ";
    return text;
}

static std::vector<std::string> LoadWords(const std::string& path){
    std::ifstream in(path);
    REQUIRE_MESSAGE(in, "cannot open ", path);
    std::vector<std::string> words;
    std::string w;
    while(in >> w)
        words.push_back(w);
    return words;
}

static const std::vector<std::string>& EnglishWords(){
    static std::vector<std::string> words = LoadWords("vigenere/wordlist-en_US/en_US.txt");
    return words;
}

static const std::vector<std::string>& GermanWords(){
    static std::vector<std::string> words = LoadWords("vigenere/wordlist-de_DE/de_DE.txt");
    return words;
}

static std::string EnglishPlain(){
    // long-encrypted.txt is known to be encrypted with FAIRY
    static std::string plain = VigenereCypher("FAIRY", Alphabet::English())
                                   .decode(ReadFile("vigenere/long-encrypted.txt"));
    return plain;
}

static std::string GermanPlain(){
    static std::string plain = VigenereCypher("MÄRCHEN", Alphabet::German())
                                   .decode(ReadFile("vigenere/german-encrypted.txt"));
    return plain;
}

// ---- Alphabet --------------------------------------------------------------

TEST_CASE("alphabet sizes"){
    CHECK(Alphabet::English().size() == 26);
    CHECK(Alphabet::German().size() == 30);
}

TEST_CASE("SymbolToChar reads A-Z, umlauts, and skips everything else"){
    Alphabet de = Alphabet::German();
    std::string text = "AZ Ä!ß";
    int i = 0;
    CHECK(de.SymbolToChar(text, i) == 'A');   // 1 byte
    CHECK(de.SymbolToChar(text, i) == 'Z');   // 1 byte
    CHECK(de.SymbolToChar(text, i) == 0);     // space
    CHECK(de.SymbolToChar(text, i) == 'A' + 26);  // Ä, 2 bytes
    CHECK(de.SymbolToChar(text, i) == 0);     // '!'
    CHECK(de.SymbolToChar(text, i) == 'A' + 29);  // ß, 2 bytes
    CHECK(i == (int)text.size());
}

TEST_CASE("CharToSymbol is the inverse of SymbolToChar"){
    Alphabet de = Alphabet::German();
    for(char c = 'A'; c < 'A' + 30; ++c){
        std::string text = de.CharToSymbol(c);
        int i = 0;
        CHECK(de.SymbolToChar(text, i) == c);
        CHECK(i == (int)text.size());
    }
}

TEST_CASE("ExtractLetters keeps only alphabet symbols"){
    CHECK(Alphabet::ExtractLetters("AB, C. 12 D!", Alphabet::English()) == "ABCD");
    // German umlauts become the internal chars after 'Z'
    std::string letters = Alphabet::ExtractLetters("KÖNIG", Alphabet::German());
    CHECK(letters.size() == 5);
    CHECK(letters[1] == 'A' + 27);  // Ö
}

TEST_CASE("LetterFrequencies sums to 1 and ranks E first in English"){
    std::vector<double> freq = Alphabet::English().LetterFrequencies(EnglishWords());
    REQUIRE(freq.size() == 26);
    double sum = 0;
    for(double f : freq) sum += f;
    CHECK(sum == doctest::Approx(1.0));
    int max_at = (int)(std::max_element(freq.begin(), freq.end()) - freq.begin());
    CHECK(max_at == 'E' - 'A');
}

TEST_CASE("LoadFrequencies uses a valid cache and rejects invalid ones"){
    const char* cache = "test_frequencies_tmp.txt";
    std::vector<std::string> few_words = {"ABBA", "BAB"};  // A: 3/7, B: 4/7

    SUBCASE("invalid cache falls back to the dictionary and rewrites it"){
        std::ofstream(cache) << "garbage";
        Alphabet en = Alphabet::English();
        en.LoadFrequencies(cache, few_words);
        CHECK(en.Frequencies()[0] == doctest::Approx(3.0 / 7.0));

        // the rewritten cache must now be valid: loading it with an empty
        // dictionary can only succeed via the cache
        Alphabet en2 = Alphabet::English();
        en2.LoadFrequencies(cache, std::vector<std::string>{"X"});
        CHECK(en2.Frequencies()[0] == doctest::Approx(3.0 / 7.0));
    }

    SUBCASE("cache for a different alphabet is rejected"){
        // write a German-sized cache (30 values)
        std::ofstream out(cache);
        for(int i = 0; i < 30; ++i)
            out << (1.0 / 30) << "\n";
        out.close();

        Alphabet en = Alphabet::English();
        en.LoadFrequencies(cache, few_words);             // must recompute
        CHECK(en.Frequencies()[0] == doctest::Approx(3.0 / 7.0));
    }

    SUBCASE("empty cache is rejected"){
        std::ofstream(cache) << "";
        Alphabet en = Alphabet::English();
        en.LoadFrequencies(cache, few_words);
        CHECK(en.Frequencies()[0] == doctest::Approx(3.0 / 7.0));
    }

    std::remove(cache);
}

// ---- VigenereCypher: encode / decode ----------------------------------------

TEST_CASE("encode/decode round trip preserves non-symbols"){
    VigenereCypher vc("LOCK", Alphabet::English());
    std::string msg = "HELLO, WORLD! 42";
    std::string enc = vc.encode(msg);
    CHECK(enc != msg);
    CHECK(vc.decode(enc) == msg);
    CHECK(enc.substr(5, 2) == ", ");   // punctuation untouched
}

TEST_CASE("german encode/decode round trip with umlauts in text and key"){
    VigenereCypher vc("MÄRCHEN", Alphabet::German());
    std::string msg = "DER KÖNIG GRÜßT";
    CHECK(vc.decode(vc.encode(msg)) == msg);
}

TEST_CASE("a shift can change the byte length of a symbol"){
    // Z (index 25) + key B (shift 1) = index 26 = Ä in the German alphabet
    VigenereCypher vc("B", Alphabet::German());
    CHECK(vc.encode("Z") == "Ä");
    CHECK(vc.decode("Ä") == "Z");
}

TEST_CASE("key with no alphabet symbols throws"){
    CHECK_THROWS_AS(VigenereCypher("123 !?", Alphabet::English()), std::runtime_error);
}

// ---- key length detection ---------------------------------------------------

TEST_CASE("Kasiski and Friedman find the key length"){
    Alphabet en = Alphabet::English();
    std::string plain = EnglishPlain();

    for(std::string key : {"LOCK", "FAIRY", "ELEPHANT"}){
        std::string cipher = VigenereCypher(key, en).encode(plain);
        std::string letters = Alphabet::ExtractLetters(cipher, en);
        CHECK(VigenereCypher::FindLengthKasiski(letters, en.size()) == (int)key.size());
        CHECK(VigenereCypher::FindLengthFriedman(letters, en.size()) == (int)key.size());
    }
}

TEST_CASE("a Caesar cipher is detected as key length 1"){
    Alphabet en = Alphabet::English();
    std::string cipher = VigenereCypher("Q", en).encode(EnglishPlain());
    std::string letters = Alphabet::ExtractLetters(cipher, en);
    CHECK(VigenereCypher::FindLengthFriedman(letters, en.size()) == 1);
}

// ---- full crack -------------------------------------------------------------

TEST_CASE("crack recovers english keys of length 1 to 10"){
    Alphabet en = Alphabet::English();
    std::string plain = EnglishPlain();

    for(std::string key : {"Q", "ON", "RED", "LOCK", "FAIRY", "PUZZLE", "MYSTERY",
                           "ELEPHANT", "BUTTERFLY", "XYLOPHONES", "WONDERLAND",
                           "ABCDEFGHIJ"}){
        std::string cipher = VigenereCypher(key, en).encode(plain);
        CHECK(VigenereCypher::Crack(cipher, EnglishWords(), en).GetKey() == key);
    }
}

TEST_CASE("crack recovers german keys including umlauts"){
    Alphabet de = Alphabet::German();
    std::string plain = GermanPlain();

    for(std::string key : {"MÄRCHEN", "SCHLÜSSEL", "GRÖßTE", "ÄÖÜß", "ZAUBERWORT", "GOLD"}){
        std::string cipher = VigenereCypher(key, de).encode(plain);
        CHECK(VigenereCypher::Crack(cipher, GermanWords(), de).GetKey() == key);
    }
}

TEST_CASE("crack decrypts the sample files"){
    Alphabet en = Alphabet::English();

    VigenereCypher s = VigenereCypher::Crack(ReadFile("vigenere/short-encrypted.txt"), EnglishWords(), en);
    CHECK(s.GetKey() == "LOCK");

    VigenereCypher l = VigenereCypher::Crack(ReadFile("vigenere/long-encrypted.txt"), EnglishWords(), en);
    CHECK(l.GetKey() == "FAIRY");

    // the medium file may report RED or REDRED (a repeated key encrypts
    // identically), so assert on the decryption instead of the key
    std::string medium = ReadFile("vigenere/medium-encrypted.txt");
    VigenereCypher m = VigenereCypher::Crack(medium, EnglishWords(), en);
    CHECK(m.decode(medium).substr(0, 16) == "ONCE UPON A TIME");

    Alphabet de = Alphabet::German();
    std::string german = ReadFile("vigenere/german-encrypted.txt");
    VigenereCypher g = VigenereCypher::Crack(german, GermanWords(), de);
    CHECK(g.GetKey() == "MÄRCHEN");
    CHECK(g.decode(german).substr(0, 13) == "ES WAR EINMAL");
}

TEST_CASE("crack works on short texts (tie-breaker territory)"){
    Alphabet en = Alphabet::English();
    std::string snippet = EnglishPlain().substr(0, 500);

    for(std::string key : {"LOCK", "FAIRY", "BUTTERFLY"}){
        std::string cipher = VigenereCypher(key, en).encode(snippet);
        CHECK(VigenereCypher::Crack(cipher, EnglishWords(), en).GetKey() == key);
    }
}

// ---- CountDictionaryWords ----------------------------------------------------

TEST_CASE("CountDictionaryWords counts exact word matches"){
    Alphabet en = Alphabet::English();
    std::unordered_set<std::string> dict = {"THE", "CAT", "SAT"};
    CHECK(VigenereCypher::CountDictionaryWords("THE CAT SAT.", dict, en) == 3);
    CHECK(VigenereCypher::CountDictionaryWords("THE CATS SAT", dict, en) == 2);  // CATS not in dict
    CHECK(VigenereCypher::CountDictionaryWords("XQZ JJJ", dict, en) == 0);
    CHECK(VigenereCypher::CountDictionaryWords("", dict, en) == 0);
}

// ---- WordSegmenter ----------------------------------------------------------

TEST_CASE("dictionary-only segmenter (no frequency file) prefers longer words"){
    Alphabet en = Alphabet::English();
    std::unordered_set<std::string> dict = {"THE", "CAT", "SAT", "ON", "MAT", "A"};
    // empty frequency path -> length^2 model
    WordSegmenter seg = WordSegmenter::Build("", dict);
    CHECK(seg.Segment("THECATSATONTHEMAT", en) == "THE CAT SAT ON THE MAT");
    CHECK(seg.Segment("A", en) == "A");
}

TEST_CASE("segmenter keeps unknown symbols as lone tokens"){
    Alphabet en = Alphabet::English();
    std::unordered_set<std::string> dict = {"THE", "CAT"};
    WordSegmenter seg = WordSegmenter::Build("", dict);
    // Q and X are in no word; they survive as single tokens around real words
    std::string out = seg.Segment("THEQXCAT", en);
    CHECK(out.substr(0, 3) == "THE");
    CHECK(out.substr(out.size() - 3) == "CAT");
}

TEST_CASE("segmenter handles german umlauts in symbol units"){
    Alphabet de = Alphabet::German();
    std::unordered_set<std::string> dict = {"KÖNIG", "GRÜßT"};
    WordSegmenter seg = WordSegmenter::Build("", dict);
    CHECK(seg.Segment("KÖNIGGRÜßT", de) == "KÖNIG GRÜßT");
}

TEST_CASE("unigram segmenter splits common english sentences"){
    // requires the frequency list; skip cleanly if it is not present
    std::ifstream freq("vigenere/wordlist-en_US/unigram_freq.txt");
    if(!freq){
        MESSAGE("unigram_freq.txt not found - skipping unigram segmentation test");
        return;
    }
    freq.close();

    Alphabet en = Alphabet::English();
    std::unordered_set<std::string> dict(EnglishWords().begin(), EnglishWords().end());
    WordSegmenter seg = WordSegmenter::Build("vigenere/wordlist-en_US/unigram_freq.txt", dict);

    CHECK(seg.Segment("THEQUICKBROWNFOXJUMPSOVERTHELAZYDOG", en)
          == "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG");
    CHECK(seg.Segment("WORDSEGMENTATIONISTHEPROBLEMOFREINSERTINGWORDBOUNDARIES", en)
          == "WORD SEGMENTATION IS THE PROBLEM OF REINSERTING WORD BOUNDARIES");
}

TEST_CASE("end-to-end: crack a spaceless ciphertext then segment it"){
    Alphabet en = Alphabet::English();

    // build a spaceless ciphertext from a known common-word sentence
    std::string plain = "THEQUICKBROWNFOXJUMPSOVERTHELAZYDOG";
    std::string repeated;            // repeat so the crack has enough text
    for(int r = 0; r < 30; ++r) repeated += plain;
    std::string cipher = VigenereCypher("SECRET", en).encode(repeated);

    VigenereCypher cracked = VigenereCypher::Crack(cipher, EnglishWords(), en);
    CHECK(cracked.GetKey() == "SECRET");

    std::string decoded = cracked.decode(cipher);
    CHECK(decoded.find(' ') == std::string::npos);   // still spaceless

    std::ifstream freq("vigenere/wordlist-en_US/unigram_freq.txt");
    if(freq){
        freq.close();
        std::unordered_set<std::string> dict(EnglishWords().begin(), EnglishWords().end());
        WordSegmenter seg = WordSegmenter::Build("vigenere/wordlist-en_US/unigram_freq.txt", dict);
        std::string segmented = seg.Segment(decoded.substr(0, plain.size()), en);
        CHECK(segmented == "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG");
    }
}
