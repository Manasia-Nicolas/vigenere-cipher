#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "file_io.hpp"
#include "segmenter.hpp"
#include "vigenere.hpp"

#include <cstdio>

// ---- helpers ---------------------------------------------------------------
// Tests run with the repository root as working directory (set in CMakeLists),
// so data files use the same relative paths as the main program.

class VigenereTestAccess {
  public:
    static std::size_t find_length_kasiski(const std::string& encoded, std::size_t symbol_count) {
        return VigenereCipher::find_length_kasiski(encoded, symbol_count);
    }

    static std::size_t find_length_friedman(const std::string& encoded,
                                            const std::vector<double>& frequencies) {
        return VigenereCipher::find_length_friedman(encoded, frequencies);
    }

    static std::string frequency_analysis(const std::string& encoded,
                                          std::size_t key_length,
                                          const std::vector<double>& frequencies) {
        return VigenereCipher::frequency_analysis(encoded, key_length, frequencies);
    }
};

static const std::vector<std::string>& english_words() {
    static std::vector<std::string> words = read_word_list("vigenere/wordlist-en_US/en_US.txt");
    return words;
}

static const std::vector<std::string>& german_words() {
    static std::vector<std::string> words = read_word_list("vigenere/wordlist-de_DE/de_DE.txt");
    return words;
}

static std::string english_plain() {
    // long-encrypted.txt is known to be encrypted with FAIRY
    static std::string plain = VigenereCipher("FAIRY", Alphabet::english())
                                   .decode(read_text_file("vigenere/data/long-encrypted.txt"));
    return plain;
}

static std::string german_plain() {
    static std::string plain = VigenereCipher("MÄRCHEN", Alphabet::german())
                                   .decode(read_text_file("vigenere/data/german-encrypted.txt"));
    return plain;
}

// ---- Alphabet --------------------------------------------------------------

TEST_CASE("alphabet sizes") {
    CHECK(Alphabet::english().size() == 26);
    CHECK(Alphabet::german().size() == 30);
}

TEST_CASE("symbol_to_char reads A-Z, umlauts, and skips everything else") {
    Alphabet de = Alphabet::german();
    std::string text = "AZ Ä!ß";
    std::size_t i = 0;
    CHECK(de.symbol_to_char(text, i) == 'A');      // 1 byte
    CHECK(de.symbol_to_char(text, i) == 'Z');      // 1 byte
    CHECK(de.symbol_to_char(text, i) == 0);        // space
    CHECK(de.symbol_to_char(text, i) == 'A' + 26); // Ä, 2 bytes
    CHECK(de.symbol_to_char(text, i) == 0);        // '!'
    CHECK(de.symbol_to_char(text, i) == 'A' + 29); // ß, 2 bytes
    CHECK(i == text.size());
}

TEST_CASE("symbol_to_char accepts suitable integral position types") {
    Alphabet de = Alphabet::german();
    std::string text = "AÄ";
    int i = 0;

    CHECK(de.symbol_to_char(text, i) == 'A');
    CHECK(de.symbol_to_char(text, i) == 'A' + 26);
    CHECK(i == static_cast<int>(text.size()));

    int negative = -1;
    CHECK_THROWS_AS(de.symbol_to_char(text, negative), std::out_of_range);
}

TEST_CASE("char_to_symbol is the inverse of symbol_to_char") {
    Alphabet de = Alphabet::german();
    for (char c = 'A'; c < 'A' + 30; ++c) {
        std::string text = de.char_to_symbol(c);
        std::size_t i = 0;
        CHECK(de.symbol_to_char(text, i) == c);
        CHECK(i == text.size());
    }
}

TEST_CASE("char_to_symbol rejects values outside the alphabet") {
    Alphabet en = Alphabet::english();
    CHECK_THROWS_AS([&] { static_cast<void>(en.char_to_symbol(0)); }(), std::out_of_range);
    CHECK_THROWS_AS([&] { static_cast<void>(en.char_to_symbol('A' + 26)); }(), std::out_of_range);
}

TEST_CASE("symbols_to_text converts a complete internal string") {
    Alphabet de = Alphabet::german();
    std::string internal = "A";
    internal += 'A' + 26;
    internal += 'A' + 29;
    CHECK(de.symbols_to_text(internal) == "AÄß");
}

TEST_CASE("extract_letters keeps only alphabet symbols") {
    Alphabet en = Alphabet::english();
    CHECK(en.extract_letters("AB, C. 12 D!") == "ABCD");
    CHECK(en.extract_letters("").empty());
    CHECK(en.extract_letters("123 !?").empty());
    // German umlauts become the internal chars after 'Z'
    std::string letters = Alphabet::german().extract_letters("KÖNIG");
    CHECK(letters.size() == 5);
    CHECK(letters[1] == 'A' + 27); // Ö
}

TEST_CASE("read_text_file preserves whitespace exactly") {
    const char* path = "test_text_file_tmp.txt";
    {
        std::ofstream output(path, std::ios::binary);
        output << "FIRST LINE\nSECOND\tLINE\n";
    }
    CHECK(read_text_file(path) == "FIRST LINE\nSECOND\tLINE\n");
    std::remove(path);
}

TEST_CASE("calculate_frequencies sums to 1 and ranks E first in English") {
    Alphabet en = Alphabet::english();
    std::vector<double> freq = LanguageResources::calculate_frequencies(en, english_words());
    REQUIRE(freq.size() == 26);
    double sum = 0;
    for (double f : freq) {
        sum += f;
    }
    CHECK(sum == doctest::Approx(1.0));
    std::size_t max_at = std::max_element(freq.begin(), freq.end()) - freq.begin();
    CHECK(max_at == static_cast<std::size_t>('E' - 'A'));
}

TEST_CASE("calculate_frequencies rejects input without alphabet symbols") {
    Alphabet en = Alphabet::english();
    CHECK_THROWS_AS(LanguageResources::calculate_frequencies(en, {}), std::runtime_error);
    CHECK_THROWS_AS(LanguageResources::calculate_frequencies(en, {"123", "!?"}),
                    std::runtime_error);
    CHECK_THROWS_AS(Alphabet::english({}, "unused-cache.txt"), std::runtime_error);
}

TEST_CASE("alphabet construction loads a valid cache and rejects invalid ones") {
    const char* cache = "test_frequencies_tmp.txt";
    std::vector<std::string> few_words = {"ABBA", "BAB"}; // A: 3/7, B: 4/7

    SUBCASE("invalid cache falls back to the dictionary and rewrites it") {
        std::ofstream(cache) << "garbage";
        Alphabet en = Alphabet::english(few_words, cache);
        CHECK(en.resources().frequencies()[0] == doctest::Approx(3.0 / 7.0));

        // the rewritten cache must now be valid: loading it with an empty
        // dictionary can only succeed via the cache
        Alphabet en2 = Alphabet::english(std::vector<std::string>{"X"}, cache);
        CHECK(en2.resources().frequencies()[0] == doctest::Approx(3.0 / 7.0));
    }

    SUBCASE("cache for a different alphabet is rejected") {
        // write a German-sized cache (30 values)
        std::ofstream out(cache);
        for (int i = 0; i < 30; ++i) {
            out << (1.0 / 30) << "\n";
        }
        out.close();

        Alphabet en = Alphabet::english(few_words, cache); // must recompute
        CHECK(en.resources().frequencies()[0] == doctest::Approx(3.0 / 7.0));
    }

    SUBCASE("empty cache is rejected") {
        std::ofstream(cache) << "";
        Alphabet en = Alphabet::english(few_words, cache);
        CHECK(en.resources().frequencies()[0] == doctest::Approx(3.0 / 7.0));
    }

    SUBCASE("negative cache values are rejected") {
        std::ofstream out(cache);
        out << "-1\n";
        for (int i = 1; i < 26; ++i) {
            out << (2.0 / 25.0) << "\n";
        }
        out.close();

        Alphabet en = Alphabet::english(few_words, cache);
        CHECK(en.resources().frequencies()[0] == doctest::Approx(3.0 / 7.0));
    }

    std::remove(cache);
}

// ---- VigenereCipher: encode / decode ----------------------------------------

TEST_CASE("encode matches the standard Vigenere example") {
    VigenereCipher vc("LEMON", Alphabet::english());
    CHECK(vc.encode("ATTACKATDAWN") == "LXFOPVEFRNHR");
    CHECK(vc.decode("LXFOPVEFRNHR") == "ATTACKATDAWN");
}

TEST_CASE("encode/decode round trip preserves non-symbols") {
    VigenereCipher vc("LOCK", Alphabet::english());
    std::string msg = "HELLO, WORLD! 42";
    std::string enc = vc.encode(msg);
    CHECK(enc != msg);
    CHECK(vc.decode(enc) == msg);
    CHECK(enc.substr(5, 2) == ", "); // punctuation untouched
}

TEST_CASE("encode handles empty input and alternate keys") {
    VigenereCipher vc("A", Alphabet::english());
    CHECK(vc.encode("").empty());
    CHECK(vc.decode("").empty());
    CHECK(vc.encode("ABC", "B") == "BCD");
    CHECK(vc.get_key() == "A");
}

TEST_CASE("german encode/decode round trip with umlauts in text and key") {
    VigenereCipher vc("MÄRCHEN", Alphabet::german());
    std::string msg = "DER KÖNIG GRÜßT";
    CHECK(vc.decode(vc.encode(msg)) == msg);
}

TEST_CASE("a shift can change the byte length of a symbol") {
    // Z (index 25) + key B (shift 1) = index 26 = Ä in the German alphabet
    VigenereCipher vc("B", Alphabet::german());
    CHECK(vc.encode("Z") == "Ä");
    CHECK(vc.decode("Ä") == "Z");
}

TEST_CASE("key with no alphabet symbols throws") {
    CHECK_THROWS_AS(VigenereCipher("123 !?", Alphabet::english()), std::runtime_error);
}

// ---- key length detection ---------------------------------------------------

TEST_CASE("Kasiski and Friedman find the key length") {
    Alphabet en = Alphabet::english();
    std::string plain = english_plain();
    std::vector<double> language_freq =
        LanguageResources::calculate_frequencies(en, english_words());

    for (std::string key : {"LOCK", "FAIRY", "ELEPHANT"}) {
        std::string cipher = VigenereCipher(key, en).encode(plain);
        std::string letters = en.extract_letters(cipher);
        CHECK(VigenereTestAccess::find_length_kasiski(letters, en.size()) == key.size());
        CHECK(VigenereTestAccess::find_length_friedman(letters, language_freq) == key.size());
    }
}

TEST_CASE("a Caesar cipher is detected as key length 1") {
    Alphabet en = Alphabet::english();
    std::string cipher = VigenereCipher("Q", en).encode(english_plain());
    std::string letters = en.extract_letters(cipher);
    CHECK(VigenereTestAccess::find_length_friedman(
              letters, LanguageResources::calculate_frequencies(en, english_words())) == 1);
}

TEST_CASE("key length detectors fall back to one for insufficient text") {
    CHECK(VigenereTestAccess::find_length_kasiski("", 26) == 1);
    CHECK(VigenereTestAccess::find_length_kasiski("ABC", 26) == 1);
    std::vector<double> uniform(26, 1.0 / 26.0);
    CHECK(VigenereTestAccess::find_length_friedman("", uniform) == 1);
    CHECK(VigenereTestAccess::find_length_friedman("A", uniform) == 1);
}

// ---- full crack -------------------------------------------------------------

TEST_CASE("crack recovers english keys of length 1 to 10") {
    Alphabet en = Alphabet::english(english_words());
    std::string plain = english_plain();

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
        std::string cipher = VigenereCipher(key, en).encode(plain);
        CHECK(VigenereCipher::crack(cipher, en).get_key() == key);
    }
}

TEST_CASE("crack recovers german keys including umlauts") {
    Alphabet de = Alphabet::german(german_words());
    std::string plain = german_plain();

    for (std::string key : {"MÄRCHEN", "SCHLÜSSEL", "GRÖßTE", "ÄÖÜß", "ZAUBERWORT", "GOLD"}) {
        std::string cipher = VigenereCipher(key, de).encode(plain);
        CHECK(VigenereCipher::crack(cipher, de).get_key() == key);
    }
}

TEST_CASE("crack decrypts the sample files") {
    Alphabet en = Alphabet::english(english_words());

    VigenereCipher s =
        VigenereCipher::crack(read_text_file("vigenere/data/short-encrypted.txt"), en);
    CHECK(s.get_key() == "LOCK");

    VigenereCipher l =
        VigenereCipher::crack(read_text_file("vigenere/data/long-encrypted.txt"), en);
    CHECK(l.get_key() == "FAIRY");

    // the medium file may report RED or REDRED (a repeated key encrypts
    // identically), so assert on the decryption instead of the key
    std::string medium = read_text_file("vigenere/data/medium-encrypted.txt");
    VigenereCipher m = VigenereCipher::crack(medium, en);
    CHECK(m.decode(medium).substr(0, 16) == "ONCE UPON A TIME");

    Alphabet de = Alphabet::german(german_words());
    std::string german = read_text_file("vigenere/data/german-encrypted.txt");
    VigenereCipher g = VigenereCipher::crack(german, de);
    CHECK(g.get_key() == "MÄRCHEN");
    CHECK(g.decode(german).substr(0, 13) == "ES WAR EINMAL");
}

TEST_CASE("crack works on short texts (tie-breaker territory)") {
    Alphabet en = Alphabet::english(english_words());
    std::string snippet = english_plain().substr(0, 500);

    for (std::string key : {"LOCK", "FAIRY", "BUTTERFLY"}) {
        std::string cipher = VigenereCipher(key, en).encode(snippet);
        CHECK(VigenereCipher::crack(cipher, en).get_key() == key);
    }
}

TEST_CASE("crack rejects an alphabet built without a word list") {
    CHECK_THROWS_AS(VigenereCipher::crack("ABC", Alphabet::english()), std::runtime_error);
}

TEST_CASE("crack rejects ciphertext without alphabet symbols") {
    Alphabet en = Alphabet::english(english_words());
    CHECK_THROWS_AS(VigenereCipher::crack("123 !?", en), std::invalid_argument);
}

// ---- score_dictionary_words ----------------------------------------------------

TEST_CASE("score_dictionary_words counts exact word matches") {
    const char* cache = "test_frequencies_cdw_tmp.txt";
    Alphabet en = Alphabet::english({"THE", "CAT", "SAT"}, cache);
    CHECK(en.resources().score_dictionary_words("THE CAT SAT.", en) == 3);
    CHECK(en.resources().score_dictionary_words("THE CATS SAT", en) == 2); // CATS not in dict
    CHECK(en.resources().score_dictionary_words("XQZ JJJ", en) == 0);
    CHECK(en.resources().score_dictionary_words("", en) == 0);

    // separators stripped: words are recovered by greedy longest match,
    // each scoring length^2 (three words of length 3 -> 27)
    CHECK(en.resources().score_dictionary_words("THECATSAT", en) == 27);
    CHECK(en.resources().score_dictionary_words("XTHEQCATSAT", en) == 27);
    CHECK(en.resources().score_dictionary_words("XQZJJJQQQQ", en) == 0);

    std::remove(cache);
}

// ---- WordSegmenter ----------------------------------------------------------

TEST_CASE("dictionary-only segmenter (no frequency file) prefers longer words") {
    Alphabet en = Alphabet::english();
    std::unordered_set<std::string> dict = {"THE", "CAT", "SAT", "ON", "MAT", "A"};
    // empty frequency path -> length^2 model
    WordSegmenter seg = WordSegmenter::build("", dict);
    CHECK(seg.segment("THECATSATONTHEMAT", en) == "THE CAT SAT ON THE MAT");
    CHECK(seg.segment("A", en) == "A");
    CHECK(seg.segment("", en).empty());
    CHECK(seg.segment("123 !?", en).empty());
}

TEST_CASE("segmenter rejects an empty dictionary") {
    CHECK_THROWS_AS(WordSegmenter::build("", {}), std::invalid_argument);
    CHECK_THROWS_AS(
        [] { static_cast<void>(WordSegmenter::segment(Alphabet::english(), "TEXT")); }(),
        std::invalid_argument);
}

TEST_CASE("segmenter keeps unknown symbols as lone tokens") {
    Alphabet en = Alphabet::english();
    std::unordered_set<std::string> dict = {"THE", "CAT"};
    WordSegmenter seg = WordSegmenter::build("", dict);
    // Q and X are in no word; they survive as single tokens around real words
    std::string out = seg.segment("THEQXCAT", en);
    CHECK(out.substr(0, 3) == "THE");
    CHECK(out.substr(out.size() - 3) == "CAT");
}

TEST_CASE("segmenter handles german umlauts in symbol units") {
    Alphabet de = Alphabet::german();
    std::unordered_set<std::string> dict = {"KÖNIG", "GRÜßT"};
    WordSegmenter seg = WordSegmenter::build("", dict);
    CHECK(seg.segment("KÖNIGGRÜßT", de) == "KÖNIG GRÜßT");
}

TEST_CASE("segmenter derives maximum word length from each language dictionary") {
    const std::string long_english = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDE";
    const std::string long_german = "ÄABCDEFGHIJKLMNOPQRSTUVWXYZABCD";

    WordSegmenter en = WordSegmenter::build("", {long_english, "THE"});
    CHECK(en.segment(long_english + "THE", Alphabet::english()) == long_english + " THE");

    WordSegmenter de = WordSegmenter::build("", {long_german, "KÖNIG"});
    CHECK(de.segment(long_german + "KÖNIG", Alphabet::german()) == long_german + " KÖNIG");
}

TEST_CASE("unigram segmenter splits common english sentences") {
    // requires the frequency list; skip cleanly if it is not present
    std::ifstream freq("vigenere/wordlist-en_US/unigram_freq.txt");
    if (!freq) {
        MESSAGE("unigram_freq.txt not found - skipping unigram segmentation test");
        return;
    }
    freq.close();

    Alphabet en = Alphabet::english();
    std::unordered_set<std::string> dict(english_words().begin(), english_words().end());
    WordSegmenter seg = WordSegmenter::build("vigenere/wordlist-en_US/unigram_freq.txt", dict);

    CHECK(seg.segment("THEQUICKBROWNFOXJUMPSOVERTHELAZYDOG", en) ==
          "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG");
    CHECK(seg.segment("WORDSEGMENTATIONISTHEPROBLEMOFREINSERTINGWORDBOUNDARIES", en) ==
          "WORD SEGMENTATION IS THE PROBLEM OF REINSERTING WORD BOUNDARIES");
}

TEST_CASE("german unigram segmenter keeps umlaut words") {
    std::ifstream freq("vigenere/wordlist-de_DE/unigram_freq.txt");
    if (!freq) { return; } // optional resource
    freq.close();

    Alphabet de = Alphabet::german();
    std::unordered_set<std::string> dict(german_words().begin(), german_words().end());
    WordSegmenter seg = WordSegmenter::build("vigenere/wordlist-de_DE/unigram_freq.txt", dict);

    CHECK(seg.segment("ESWAREINMALEINKÖNIG", de) == "ES WAR EINMAL EIN KÖNIG");
    CHECK(seg.segment("DIESCHÖNEMÜLLERSTOCHTER", de) == "DIE SCHÖNE MÜLLERS TOCHTER");
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

    Alphabet en = Alphabet::english();
    std::unordered_set<std::string> dict = {"A", "LONG", "ALONG"};
    WordSegmenter seg = WordSegmenter::build(frequencies, dict);
    CHECK(seg.segment("ALONG", en) == "A LONG");

    std::remove(frequencies);
}

TEST_CASE("end-to-end: crack a spaceless ciphertext then segment it") {
    Alphabet en = Alphabet::english(english_words());
    std::string cipher = read_text_file("vigenere/data/spaceless-encrypted.txt");

    VigenereCipher cracked = VigenereCipher::crack(cipher, en);
    CHECK(cracked.get_key() == "SECRET");

    std::string decoded = cracked.decode(cipher);
    std::string letters = en.extract_letters(decoded);
    CHECK(decoded.back() == '\n');
    CHECK(letters.size() + 1 == decoded.size());

    std::ifstream freq("vigenere/wordlist-en_US/unigram_freq.txt");
    if (freq) {
        freq.close();
        std::string expected_symbols = "BYTHESIDEOFAWOODINACOUNTRY";
        std::string segmented =
            WordSegmenter::segment(en, letters.substr(0, expected_symbols.size()));
        CHECK(segmented == "BY THE SIDE OF A WOOD IN A COUNTRY");
    }
}
