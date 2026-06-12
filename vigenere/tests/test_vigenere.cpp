#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "segmenter.hpp"
#include "vigenere.hpp"

#include <cstdio>

// ---- helpers ---------------------------------------------------------------
// Tests run with the repository root as working directory (set in CMakeLists),
// so data files use the same relative paths as the main program.

static std::string ReadFile(const std::string& path) {
    std::ifstream in(path);
    REQUIRE_MESSAGE(in, "cannot open ", path, " (tests must run from the repo root)");
    std::string text, line;
    while (std::getline(in, line))
        text += line + " ";
    return text;
}

static std::vector<std::string> LoadWords(const std::string& path) {
    std::ifstream in(path);
    REQUIRE_MESSAGE(in, "cannot open ", path);
    std::vector<std::string> words;
    std::string w;
    while (in >> w)
        words.push_back(w);
    return words;
}

static const std::vector<std::string>& EnglishWords() {
    static std::vector<std::string> words = LoadWords("vigenere/wordlist-en_US/en_US.txt");
    return words;
}

static const std::vector<std::string>& GermanWords() {
    static std::vector<std::string> words = LoadWords("vigenere/wordlist-de_DE/de_DE.txt");
    return words;
}

static std::string EnglishPlain() {
    // long-encrypted.txt is known to be encrypted with FAIRY
    static std::string plain = VigenereCypher("FAIRY", Alphabet::English())
                                   .decode(ReadFile("vigenere/data/long-encrypted.txt"));
    return plain;
}

static std::string GermanPlain() {
    static std::string plain = VigenereCypher("MÄRCHEN", Alphabet::German())
                                   .decode(ReadFile("vigenere/data/german-encrypted.txt"));
    return plain;
}

// ---- Alphabet --------------------------------------------------------------

TEST_CASE("alphabet sizes") {
    CHECK(Alphabet::English().size() == 26);
    CHECK(Alphabet::German().size() == 30);
}

TEST_CASE("SymbolToChar reads A-Z, umlauts, and skips everything else") {
    Alphabet de = Alphabet::German();
    std::string text = "AZ Ä!ß";
    int i = 0;
    CHECK(de.SymbolToChar(text, i) == 'A');      // 1 byte
    CHECK(de.SymbolToChar(text, i) == 'Z');      // 1 byte
    CHECK(de.SymbolToChar(text, i) == 0);        // space
    CHECK(de.SymbolToChar(text, i) == 'A' + 26); // Ä, 2 bytes
    CHECK(de.SymbolToChar(text, i) == 0);        // '!'
    CHECK(de.SymbolToChar(text, i) == 'A' + 29); // ß, 2 bytes
    CHECK(i == (int)text.size());
}

TEST_CASE("CharToSymbol is the inverse of SymbolToChar") {
    Alphabet de = Alphabet::German();
    for (char c = 'A'; c < 'A' + 30; ++c) {
        std::string text = de.CharToSymbol(c);
        int i = 0;
        CHECK(de.SymbolToChar(text, i) == c);
        CHECK(i == (int)text.size());
    }
}

TEST_CASE("SymbolsToText converts a complete internal string") {
    Alphabet de = Alphabet::German();
    std::string internal = "A";
    internal += 'A' + 26;
    internal += 'A' + 29;
    CHECK(de.SymbolsToText(internal) == "AÄß");
}

TEST_CASE("ExtractLetters keeps only alphabet symbols") {
    Alphabet en = Alphabet::English();
    CHECK(en.ExtractLetters("AB, C. 12 D!") == "ABCD");
    CHECK(en.ExtractLetters("").empty());
    CHECK(en.ExtractLetters("123 !?").empty());
    // German umlauts become the internal chars after 'Z'
    std::string letters = Alphabet::German().ExtractLetters("KÖNIG");
    CHECK(letters.size() == 5);
    CHECK(letters[1] == 'A' + 27); // Ö
}

TEST_CASE("LetterFrequencies sums to 1 and ranks E first in English") {
    std::vector<double> freq = Alphabet::English().LetterFrequencies(EnglishWords());
    REQUIRE(freq.size() == 26);
    double sum = 0;
    for (double f : freq)
        sum += f;
    CHECK(sum == doctest::Approx(1.0));
    int max_at = (int)(std::max_element(freq.begin(), freq.end()) - freq.begin());
    CHECK(max_at == 'E' - 'A');
}

TEST_CASE("alphabet construction loads a valid cache and rejects invalid ones") {
    const char* cache = "test_frequencies_tmp.txt";
    std::vector<std::string> few_words = {"ABBA", "BAB"}; // A: 3/7, B: 4/7

    SUBCASE("invalid cache falls back to the dictionary and rewrites it") {
        std::ofstream(cache) << "garbage";
        Alphabet en = Alphabet::English(few_words, cache);
        CHECK(en.Frequencies()[0] == doctest::Approx(3.0 / 7.0));

        // the rewritten cache must now be valid: loading it with an empty
        // dictionary can only succeed via the cache
        Alphabet en2 = Alphabet::English(std::vector<std::string>{"X"}, cache);
        CHECK(en2.Frequencies()[0] == doctest::Approx(3.0 / 7.0));
    }

    SUBCASE("cache for a different alphabet is rejected") {
        // write a German-sized cache (30 values)
        std::ofstream out(cache);
        for (int i = 0; i < 30; ++i)
            out << (1.0 / 30) << "\n";
        out.close();

        Alphabet en = Alphabet::English(few_words, cache); // must recompute
        CHECK(en.Frequencies()[0] == doctest::Approx(3.0 / 7.0));
    }

    SUBCASE("empty cache is rejected") {
        std::ofstream(cache) << "";
        Alphabet en = Alphabet::English(few_words, cache);
        CHECK(en.Frequencies()[0] == doctest::Approx(3.0 / 7.0));
    }

    std::remove(cache);
}

// ---- VigenereCypher: encode / decode ----------------------------------------

TEST_CASE("encode matches the standard Vigenere example") {
    VigenereCypher vc("LEMON", Alphabet::English());
    CHECK(vc.encode("ATTACKATDAWN") == "LXFOPVEFRNHR");
    CHECK(vc.decode("LXFOPVEFRNHR") == "ATTACKATDAWN");
}

TEST_CASE("encode/decode round trip preserves non-symbols") {
    VigenereCypher vc("LOCK", Alphabet::English());
    std::string msg = "HELLO, WORLD! 42";
    std::string enc = vc.encode(msg);
    CHECK(enc != msg);
    CHECK(vc.decode(enc) == msg);
    CHECK(enc.substr(5, 2) == ", "); // punctuation untouched
}

TEST_CASE("encode handles empty input and alternate keys") {
    VigenereCypher vc("A", Alphabet::English());
    CHECK(vc.encode("").empty());
    CHECK(vc.decode("").empty());
    CHECK(vc.encode("ABC", "B") == "BCD");
    CHECK(vc.GetKey() == "A");
}

TEST_CASE("german encode/decode round trip with umlauts in text and key") {
    VigenereCypher vc("MÄRCHEN", Alphabet::German());
    std::string msg = "DER KÖNIG GRÜßT";
    CHECK(vc.decode(vc.encode(msg)) == msg);
}

TEST_CASE("a shift can change the byte length of a symbol") {
    // Z (index 25) + key B (shift 1) = index 26 = Ä in the German alphabet
    VigenereCypher vc("B", Alphabet::German());
    CHECK(vc.encode("Z") == "Ä");
    CHECK(vc.decode("Ä") == "Z");
}

TEST_CASE("key with no alphabet symbols throws") {
    CHECK_THROWS_AS(VigenereCypher("123 !?", Alphabet::English()), std::runtime_error);
}

// ---- key length detection ---------------------------------------------------

TEST_CASE("Kasiski and Friedman find the key length") {
    Alphabet en = Alphabet::English();
    std::string plain = EnglishPlain();
    std::vector<double> language_freq = en.LetterFrequencies(EnglishWords());

    for (std::string key : {"LOCK", "FAIRY", "ELEPHANT"}) {
        std::string cipher = VigenereCypher(key, en).encode(plain);
        std::string letters = en.ExtractLetters(cipher);
        CHECK(VigenereCypher::FindLengthKasiski(letters, en.size()) == (int)key.size());
        CHECK(VigenereCypher::FindLengthFriedman(letters, language_freq) == (int)key.size());
    }
}

TEST_CASE("a Caesar cipher is detected as key length 1") {
    Alphabet en = Alphabet::English();
    std::string cipher = VigenereCypher("Q", en).encode(EnglishPlain());
    std::string letters = en.ExtractLetters(cipher);
    CHECK(VigenereCypher::FindLengthFriedman(letters, en.LetterFrequencies(EnglishWords())) == 1);
}

TEST_CASE("key length detectors fall back to one for insufficient text") {
    CHECK(VigenereCypher::FindLengthKasiski("", 26) == 1);
    CHECK(VigenereCypher::FindLengthKasiski("ABC", 26) == 1);
    std::vector<double> uniform(26, 1.0 / 26.0);
    CHECK(VigenereCypher::FindLengthFriedman("", uniform) == 1);
    CHECK(VigenereCypher::FindLengthFriedman("A", uniform) == 1);
}

// ---- full crack -------------------------------------------------------------

TEST_CASE("crack recovers english keys of length 1 to 10") {
    Alphabet en = Alphabet::English(EnglishWords());
    std::string plain = EnglishPlain();

    for (std::string key : {"Q",
                            "ON",
                            "RED",
                            "LOCK",
                            "FAIRY",
                            "PUZZLE",
                            "MYSTERY",
                            "ELEPHANT",
                            "BUTTERFLY",
                            "XYLOPHONES",
                            "WONDERLAND",
                            "ABCDEFGHIJ"}) {
        std::string cipher = VigenereCypher(key, en).encode(plain);
        CHECK(VigenereCypher::Crack(cipher, en).GetKey() == key);
    }
}

TEST_CASE("crack recovers german keys including umlauts") {
    Alphabet de = Alphabet::German(GermanWords());
    std::string plain = GermanPlain();

    for (std::string key : {"MÄRCHEN", "SCHLÜSSEL", "GRÖßTE", "ÄÖÜß", "ZAUBERWORT", "GOLD"}) {
        std::string cipher = VigenereCypher(key, de).encode(plain);
        CHECK(VigenereCypher::Crack(cipher, de).GetKey() == key);
    }
}

TEST_CASE("crack decrypts the sample files") {
    Alphabet en = Alphabet::English(EnglishWords());

    VigenereCypher s = VigenereCypher::Crack(ReadFile("vigenere/data/short-encrypted.txt"), en);
    CHECK(s.GetKey() == "LOCK");

    VigenereCypher l = VigenereCypher::Crack(ReadFile("vigenere/data/long-encrypted.txt"), en);
    CHECK(l.GetKey() == "FAIRY");

    // the medium file may report RED or REDRED (a repeated key encrypts
    // identically), so assert on the decryption instead of the key
    std::string medium = ReadFile("vigenere/data/medium-encrypted.txt");
    VigenereCypher m = VigenereCypher::Crack(medium, en);
    CHECK(m.decode(medium).substr(0, 16) == "ONCE UPON A TIME");

    Alphabet de = Alphabet::German(GermanWords());
    std::string german = ReadFile("vigenere/data/german-encrypted.txt");
    VigenereCypher g = VigenereCypher::Crack(german, de);
    CHECK(g.GetKey() == "MÄRCHEN");
    CHECK(g.decode(german).substr(0, 13) == "ES WAR EINMAL");
}

TEST_CASE("crack works on short texts (tie-breaker territory)") {
    Alphabet en = Alphabet::English(EnglishWords());
    std::string snippet = EnglishPlain().substr(0, 500);

    for (std::string key : {"LOCK", "FAIRY", "BUTTERFLY"}) {
        std::string cipher = VigenereCypher(key, en).encode(snippet);
        CHECK(VigenereCypher::Crack(cipher, en).GetKey() == key);
    }
}

TEST_CASE("crack rejects an alphabet built without a word list") {
    CHECK_THROWS_AS(VigenereCypher::Crack("ABC", Alphabet::English()), std::runtime_error);
}

// ---- CountDictionaryWords ----------------------------------------------------

TEST_CASE("CountDictionaryWords counts exact word matches") {
    const char* cache = "test_frequencies_cdw_tmp.txt";
    Alphabet en = Alphabet::English({"THE", "CAT", "SAT"}, cache);
    CHECK(en.CountDictionaryWords("THE CAT SAT.") == 3);
    CHECK(en.CountDictionaryWords("THE CATS SAT") == 2); // CATS not in dict
    CHECK(en.CountDictionaryWords("XQZ JJJ") == 0);
    CHECK(en.CountDictionaryWords("") == 0);

    // separators stripped: words are recovered by greedy longest match,
    // each scoring length^2 (three words of length 3 -> 27)
    CHECK(en.CountDictionaryWords("THECATSAT") == 27);
    CHECK(en.CountDictionaryWords("XTHEQCATSAT") == 27);
    CHECK(en.CountDictionaryWords("XQZJJJQQQQ") == 0);

    std::remove(cache);
}

// ---- WordSegmenter ----------------------------------------------------------

TEST_CASE("dictionary-only segmenter (no frequency file) prefers longer words") {
    Alphabet en = Alphabet::English();
    std::unordered_set<std::string> dict = {"THE", "CAT", "SAT", "ON", "MAT", "A"};
    // empty frequency path -> length^2 model
    WordSegmenter seg = WordSegmenter::Build("", dict);
    CHECK(seg.Segment("THECATSATONTHEMAT", en) == "THE CAT SAT ON THE MAT");
    CHECK(seg.Segment("A", en) == "A");
    CHECK(seg.Segment("", en).empty());
    CHECK(seg.Segment("123 !?", en).empty());
}

TEST_CASE("segmenter keeps unknown symbols as lone tokens") {
    Alphabet en = Alphabet::English();
    std::unordered_set<std::string> dict = {"THE", "CAT"};
    WordSegmenter seg = WordSegmenter::Build("", dict);
    // Q and X are in no word; they survive as single tokens around real words
    std::string out = seg.Segment("THEQXCAT", en);
    CHECK(out.substr(0, 3) == "THE");
    CHECK(out.substr(out.size() - 3) == "CAT");
}

TEST_CASE("segmenter handles german umlauts in symbol units") {
    Alphabet de = Alphabet::German();
    std::unordered_set<std::string> dict = {"KÖNIG", "GRÜßT"};
    WordSegmenter seg = WordSegmenter::Build("", dict);
    CHECK(seg.Segment("KÖNIGGRÜßT", de) == "KÖNIG GRÜßT");
}

TEST_CASE("segmenter derives maximum word length from each language dictionary") {
    const std::string long_english = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDE";
    const std::string long_german = "ÄABCDEFGHIJKLMNOPQRSTUVWXYZABCD";

    WordSegmenter en = WordSegmenter::Build("", {long_english, "THE"});
    CHECK(en.Segment(long_english + "THE", Alphabet::English()) == long_english + " THE");

    WordSegmenter de = WordSegmenter::Build("", {long_german, "KÖNIG"});
    CHECK(de.Segment(long_german + "KÖNIG", Alphabet::German()) == long_german + " KÖNIG");
}

TEST_CASE("unigram segmenter splits common english sentences") {
    // requires the frequency list; skip cleanly if it is not present
    std::ifstream freq("vigenere/wordlist-en_US/unigram_freq.txt");
    if (!freq) {
        MESSAGE("unigram_freq.txt not found - skipping unigram segmentation test");
        return;
    }
    freq.close();

    Alphabet en = Alphabet::English();
    std::unordered_set<std::string> dict(EnglishWords().begin(), EnglishWords().end());
    WordSegmenter seg = WordSegmenter::Build("vigenere/wordlist-en_US/unigram_freq.txt", dict);

    CHECK(seg.Segment("THEQUICKBROWNFOXJUMPSOVERTHELAZYDOG", en) ==
          "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG");
    CHECK(seg.Segment("WORDSEGMENTATIONISTHEPROBLEMOFREINSERTINGWORDBOUNDARIES", en) ==
          "WORD SEGMENTATION IS THE PROBLEM OF REINSERTING WORD BOUNDARIES");
}

TEST_CASE("german unigram segmenter keeps umlaut words") {
    std::ifstream freq("vigenere/wordlist-de_DE/unigram_freq.txt");
    if (!freq) return; // optional resource
    freq.close();

    Alphabet de = Alphabet::German();
    std::unordered_set<std::string> dict(GermanWords().begin(), GermanWords().end());
    WordSegmenter seg = WordSegmenter::Build("vigenere/wordlist-de_DE/unigram_freq.txt", dict);

    CHECK(seg.Segment("ESWAREINMALEINKÖNIG", de) == "ES WAR EINMAL EIN KÖNIG");
    CHECK(seg.Segment("DIESCHÖNEMÜLLERSTOCHTER", de) == "DIE SCHÖNE MÜLLERS TOCHTER");
}

TEST_CASE("unigram segmenter normalizes ASCII words and prefers common splits") {
    const char* frequencies = "test_unigram_tmp.txt";
    {
        std::ofstream out(frequencies);
        out << "a 1000\n";
        out << "long 1000\n";
        out << "along 1\n";
        out << "invalid-word 999999\n";
        out << "zero 0\n";
    }

    Alphabet en = Alphabet::English();
    std::unordered_set<std::string> dict = {"A", "LONG", "ALONG"};
    WordSegmenter seg = WordSegmenter::Build(frequencies, dict);
    CHECK(seg.Segment("ALONG", en) == "A LONG");

    std::remove(frequencies);
}

TEST_CASE("end-to-end: crack a spaceless ciphertext then segment it") {
    Alphabet en = Alphabet::English(EnglishWords());
    std::string cipher = ReadFile("vigenere/data/spaceless-encrypted.txt");

    VigenereCypher cracked = VigenereCypher::Crack(cipher, en);
    CHECK(cracked.GetKey() == "SECRET");

    std::string decoded = cracked.decode(cipher);
    std::string letters = en.ExtractLetters(decoded);
    CHECK(letters.size() + 1 == decoded.size()); // only ReadFile's trailing space

    std::ifstream freq("vigenere/wordlist-en_US/unigram_freq.txt");
    if (freq) {
        freq.close();
        std::string expected_symbols = "BYTHESIDEOFAWOODINACOUNTRY";
        std::string segmented =
            WordSegmenter::Segment(en, letters.substr(0, expected_symbols.size()));
        CHECK(segmented == "BY THE SIDE OF A WOOD IN A COUNTRY");
    }
}
