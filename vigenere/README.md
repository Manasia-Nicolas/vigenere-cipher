# Vigenere Cipher

A modern C++ implementation of the Vigenere cipher for English and German.
The project can:

- encrypt and decrypt text with a known key;
- recover an unknown key using Kasiski examination, the Friedman test, and
  frequency analysis;
- preserve spaces, punctuation, and unsupported characters;
- restore word boundaries when spaces were removed before encryption.

C++23 is the default. The project also supports C++20.

## Project Rubric Mapping

| Requirement | Implementation evidence |
|-------------|-------------------------|
| Basic: decrypt English Vigenere ciphertext | `VigenereCipher::crack` recovers the key and returns a cipher that decrypts the message. |
| Basic: Kasiski examination | Private `find_length_kasiski` estimates key length from repeated trigram distances. |
| Basic: frequency analysis | Private `frequency_analysis` recovers each key symbol using chi-squared comparison with language frequencies. |
| Improvement: encrypter and round-trip verification | `encode` implements encryption; tests verify known answers and encode/decode round trips. |
| Improvement: Friedman test | Private `find_length_friedman` estimates key length from the index of coincidence. |
| Improvement: keys up to 10 characters | `max_key_length` is `10`; tests recover English keys across lengths 1-10. |
| Advanced: messages without spaces or special characters | `WordSegmenter` restores likely word boundaries using dynamic programming. |
| Advanced: German support | `Alphabet::german` supports A-Z, Ä, Ö, Ü, and ß with modulo-30 arithmetic. |
| Extra: unit tests | Doctest unit tests and CTest CLI integration tests cover algorithms, edge cases, and invalid input. |
| Extra: documentation | This README documents design, algorithms, complexity, limitations, and usage. |
| Extra: linter and formatting | `.clang-tidy`, `.clang-format`, and compiler warning flags are configured. |
| Extra: performance analysis | `bench/bench_vigenere.cpp` measures algorithm scaling; complexity is documented below. |

## Supported Languages

| Language | CLI value | Alphabet | Arithmetic |
|----------|-----------|----------|------------|
| English | `en` | A-Z | modulo 26 |
| German | `de` | A-Z, Ä, Ö, Ü, ß | modulo 30 |

German extra symbols are treated as complete cipher symbols even though they
use multiple bytes in UTF-8.

Input text and keys should be uppercase. Unsupported characters are preserved
in messages but ignored in keys and statistical analysis.

## Build and Run

Run all commands from the repository root:

```sh
cmake -S . -B build -DPROJECT_TOPIC=VIGENERE
cmake --build build
./build/vigenere/vigenere [en|de] [encrypted-file]
```

Both command-line arguments are optional. The defaults are English and
`vigenere/data/long-encrypted.txt`.

Example output:

```text
1.LOCK
2.WE MET NEXT DAY AS HE HAD ARRANGED, ...
```

- `1.` is the recovered key.
- `2.` is the decrypted message.
- `3.` is printed when the program detects spaceless plaintext and restores
  likely word boundaries.

## Project Structure

| Path | Responsibility |
|------|----------------|
| `src/main.cpp` | Command-line application |
| `src/alphabet.hpp` | Alphabet definitions and UTF-8 symbol conversion |
| `src/language_resources.hpp/.cpp` | Dictionary, language frequencies, cache loading, and plaintext scoring |
| `src/vigenere.hpp` | Encryption, decryption, and key recovery |
| `src/segmenter.hpp` | Dynamic-programming word segmentation |
| `src/file_io.hpp` | Text-file and word-list loading |
| `tests/test_vigenere.cpp` | Doctest unit and integration tests |
| `bench/bench_vigenere.cpp` | Standalone algorithm benchmark |
| `data/` | Example ciphertexts |
| `wordlist-en_US/`, `wordlist-de_DE/` | Language dictionaries and frequency data |

## Design

### `Alphabet`

`Alphabet` defines which symbols belong to a language and converts between
UTF-8 text and the compact representation used by the cipher.

Internally, every symbol occupies one `char`:

```text
Text:       A    Z    Ä    Ö    Ü    ß
Internal:  'A'  'Z'  '['  '\'  ']'  '^'
```

This lets the cryptographic algorithms treat German extra symbols exactly like
A-Z.

Important methods:

| Method | Purpose |
|--------|---------|
| `english()`, `german()` | Create lightweight alphabets for encryption and decryption |
| `english(words, ...)`, `german(words, ...)` | Create alphabets with language resources required for cracking |
| `size()` | Return the number of supported symbols |
| `symbol_to_char(text, position)` | Read one symbol and advance the byte position |
| `char_to_symbol(character)` | Convert one internal value back to UTF-8 |
| `extract_letters(text)` | Keep supported symbols and return their internal representation |
| `resources()` | Access the composed `LanguageResources` object |

`symbol_to_char` accepts mutable integral position types that make sense for
indexing, including `std::size_t` and `int`:

```cpp
Alphabet alphabet = Alphabet::german();
std::string text = "AÄ";
int position = 0;

char first = alphabet.symbol_to_char(text, position);  // 'A', position == 1
char second = alphabet.symbol_to_char(text, position); // internal Ä, position == 3
```

The method rejects negative positions, out-of-range positions, `bool`, and
positions that cannot fit back into the caller's integer type. Internally,
container positions remain `std::size_t`.

### `LanguageResources`

`LanguageResources` owns data needed for statistical key recovery:

- a dictionary stored in `std::unordered_set`;
- a language symbol-frequency distribution;
- the longest dictionary word;
- the path to a unigram-frequency file.

When constructed, it tries to load cached symbol frequencies. A cache is valid
only when it contains exactly one value per alphabet symbol and sums to
approximately `1`. Otherwise, frequencies are recalculated from the dictionary
and the cache is rewritten.

`score_dictionary_words` evaluates candidate decryptions. Normal text is
scored by counting dictionary words. For text without spaces, it searches for
dictionary matches and gives longer words more weight.

### `VigenereCipher`

`VigenereCipher` stores an `Alphabet` and a key in the internal
one-value-per-symbol representation.

Public operations:

| Method | Purpose |
|--------|---------|
| `VigenereCipher(key, alphabet)` | Construct a cipher with a known key |
| `encode(message)` | Encrypt using the stored key |
| `encode(message, other_key)` | Encrypt with another key without modifying the object |
| `decode(message)` | Decrypt using the stored key |
| `get_key()` | Return the key as UTF-8 text |
| `crack(encoded, alphabet)` | Recover an unknown key using language resources |

Encryption and decryption apply modular arithmetic only to supported symbols:

```text
encrypted = (plaintext + key) mod alphabet_size
decrypted = (ciphertext - key + alphabet_size) mod alphabet_size
```

Unsupported characters are copied unchanged, and they do not advance the key
position. File input preserves spaces, tabs, and newlines exactly.

#### Key Recovery

`crack` performs the following steps:

1. Convert ciphertext to the internal symbol representation.
2. Estimate the key length with Kasiski examination.
3. Estimate the key length with the Friedman index of coincidence.
4. Recover key symbols with per-column frequency analysis.
5. If the length estimators disagree, decrypt both candidates and use
   dictionary scoring to choose the more plausible plaintext.

The key-recovery helpers are private because they are implementation details of
`crack`. Tests and benchmarks access them through narrow friend accessor
classes without expanding the production API.

The current maximum tested key length is `10`.

### `WordSegmenter`

`WordSegmenter` restores spaces in text such as:

```text
THEVIGENERECIPHERISAMETHOD
```

It uses dynamic programming to find the sequence of words with the lowest
total cost.

The CLI detects spaceless plaintext when the decrypted content, excluding
trailing file whitespace, consists entirely of alphabet symbols. This works
for short and long messages.

When unigram counts are available, a word receives the negative
log-probability cost:

```text
cost(word) = -log10(count(word) / total_count)
           = log10(total_count) - log10(count(word))
```

Common words have lower costs. Logarithms allow the algorithm to add word
costs instead of multiplying probabilities.

If no valid frequency file exists, the segmenter falls back to:

```text
cost(word) = -(symbol_length * symbol_length)
```

This rewards longer dictionary matches. Unknown single symbols remain allowed
at a high cost so segmentation always succeeds.

The maximum considered word length is derived from the selected language's
resources and measured in symbols, not UTF-8 bytes.

## Standard Library Use

The implementation relies on standard-library value types and RAII:

| Facility | Use |
|----------|-----|
| `std::string`, `std::vector` | Text, symbols, frequencies, and dynamic-programming tables |
| `std::unordered_map`, `std::unordered_set` | Word costs and dictionary lookup |
| `std::ifstream`, `std::ofstream` | Automatically managed file resources |
| `std::size_t` | Internal container indexes, positions, lengths, and counts |
| Concepts such as `std::integral` | Constrain the generic `symbol_to_char` index overload |
| `<algorithm>`, `<cmath>`, `<chrono>`, `<random>` | Analysis, scoring, timing, and benchmark generation |

No manual memory management or smart pointers are needed. Objects own their
resources directly, and standard-library containers and streams release them
automatically through RAII.

The project does not use runtime polymorphism. The constrained
`symbol_to_char` overload and benchmark timing function use compile-time
generic programming.

## Complexity Analysis

Symbols used below:

| Symbol | Meaning |
|--------|---------|
| `n` | Number of text or ciphertext symbols |
| `A` | Alphabet size, 26 or 30 |
| `K` | Maximum tested key length, currently 10 |
| `k` | One selected key length |
| `L` | Longest known word |
| `D` | Total size of the dictionary |
| `U` | Total size of the unigram-frequency input |

| Operation | Time | Extra space |
|-----------|------|-------------|
| `symbol_to_char` | `O(A)` | `O(1)` |
| `extract_letters` | `O(n * A)` | `O(n)` |
| `encode`, `decode` | `O(n * A)` | `O(n)` |
| `calculate_frequencies` | `O(D * A)` | `O(A)` |
| Kasiski examination | `O(n * K)` | `O(n + A^3)` |
| Friedman test | `O(K * n + A * K^2)` | `O(A + K)` |
| Frequency analysis | `O(n + k * A^2)` | `O(A + k)` |
| Dictionary scoring | `O(n * L^2)` worst case | `O(L)` |
| `WordSegmenter::build` | `O(U + D)` average | `O(U + D)` |
| `WordSegmenter::segment` | `O(n * L^2)` worst case | `O(n + L)` |

Because `A` and `K` are small fixed values, encryption, decryption, and the
main cracking algorithms behave effectively linearly with text length.
Dictionary loading and segmentation can dominate for large language resources.

## Testing

The project uses the single-header Doctest framework for unit tests and CTest
for running the complete suite.

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests cover:

- English and German alphabet conversion;
- multiple integral index types for `symbol_to_char`;
- exact preservation of file whitespace;
- encryption and decryption, including UTF-8 symbols;
- Kasiski, Friedman, and frequency-analysis helpers;
- complete key recovery;
- frequency-cache validation;
- dictionary scoring and word segmentation;
- rejected invalid inputs and unsupported configurations;
- CLI execution against sample files.

Run with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake -S . -B build-san -DPROJECT_TOPIC=VIGENERE -DVIGENERE_SANITIZE=ON
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

## Benchmark

The standalone benchmark measures how the key-recovery algorithms scale with
synthetic ciphertext:

```sh
cmake --build build --target vigenere_bench
./build/vigenere/vigenere_bench
```

## Formatting and Static Analysis

Formatting:

```sh
clang-format --dry-run --Werror vigenere/src/*.cpp vigenere/src/*.hpp \
  vigenere/{tests/test_vigenere.cpp,bench/bench_vigenere.cpp}
```

Static analysis on macOS with Homebrew LLVM:

```sh
SDK="$(xcrun --show-sdk-path)"
/opt/homebrew/opt/llvm/bin/clang-tidy -p build \
  vigenere/src/*.cpp vigenere/src/*.hpp \
  --extra-arg=-isysroot --extra-arg="$SDK" \
  --extra-arg=-isystem --extra-arg="$SDK/usr/include/c++/v1"
```

More tooling details are available in [`TOOLING.md`](TOOLING.md).
The README explains the project and its design; `TOOLING.md` keeps detailed
developer commands, sanitizer instructions, and performance results separate.

## Limitations

- Input should be uppercase; lowercase message letters are preserved rather
  than encrypted.
- Automatic cracking only tests key lengths from 1 to 10.
- Statistical key recovery requires enough ciphertext to identify language
  patterns reliably.
- The selected language must match the alphabet and language used for
  encryption.
