# Testing, Linting and Performance

All commands are run from the **repository root** (the directory that holds the
top-level `CMakeLists.txt`).

This document is the developer-facing companion to `README.md`. The README
explains the program, design, algorithms, and usage. This file keeps detailed
testing, static-analysis, formatting, sanitizer, and performance workflows in
one place without making the main README harder to scan.

## Layout

```
vigenere/
  src/
    alphabet.hpp      alphabet symbols and UTF-8 conversion
    language_resources.hpp/.cpp  dictionary, frequencies, and scoring
    file_io.hpp       reusable text and word-list loading
    vigenere.hpp      VigenereCipher encryption and cracking logic
    segmenter.hpp     dynamic-programming word-boundary recovery
    main.cpp          CLI front-end
  tests/
    doctest.h         single-header test framework (vendored)
    test_vigenere.cpp unit + integration tests
  bench/
    bench_vigenere.cpp  scaling benchmark (no framework)
  data/               sample encrypted text files
  .clang-format       formatting rules
  .clang-tidy         static-analysis checks
```

The cracking building blocks are private implementation details of
`VigenereCipher`. The test and benchmark translation units define narrow friend
accessors so they can exercise those algorithms directly without expanding the
production public API.

## 1. Unit testing — doctest + CTest

Configure once, then build and test:

```sh
cmake -S . -B build -DPROJECT_TOPIC=VIGENERE
cmake --build build
ctest --test-dir build --output-on-failure
```

The suite (`tests/test_vigenere.cpp`) covers:

- **Alphabet**: sizes, UTF-8 `symbol_to_char`/`char_to_symbol` round-trips,
  `extract_letters`, exact file-whitespace preservation, dictionary
  `calculate_frequencies`, and construction-time frequency loading (valid
  cache reused; empty / garbage / negative / wrong-alphabet caches rejected
  and recomputed).
- **Cipher**: `encode`/`decode` round trips (including German umlauts in both
  text and key), the standard `ATTACKATDAWN` known-answer example,
  alternate-key and empty-input cases, the length-changing shift `Z + B = Ä`,
  and the empty-key exception.
- **Key length**: Kasiski and Friedman agree with the true length for several
  keys; a Caesar cipher is detected as length 1.
- **Full crack**: all English key lengths 1–10, German keys including the
  all-umlaut `ÄÖÜß`, the four sample files, and short-text (tie-breaker) cases.
- **Word segmentation**: dictionary-only and unigram cost models, unknown
  symbols, empty input, rejected empty dictionaries, German UTF-8 symbols,
  controlled unigram normalization, and cracking then segmenting spaceless
  text.
- **Validation**: invalid internal symbols, empty keys, unsupported cracking
  inputs, invalid frequency data, and missing language resources.
- **CLI integration**: normal and spaceless sample files run through the built
  executable, verifying the organized `data/` paths, preserved file whitespace,
  and expected output.

The test target runs with the repo root as its working directory (set via
`WORKING_DIRECTORY` in CMake), so it reads the same data files as the program.

### Sanitizers (recommended before trusting a refactor)

```sh
cmake -S . -B build-san -DPROJECT_TOPIC=VIGENERE -DVIGENERE_SANITIZE=ON
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

This builds the library, CLI, tests, and benchmark with AddressSanitizer +
UndefinedBehaviorSanitizer, then runs all unit and CLI integration tests.
It catches out-of-bounds reads (e.g. a stray non-symbol reaching
`find_length_kasiski`) and integer/UB issues that normal runs pass over.

### C++20 compatibility

The default standard is C++23. A separate configuration verifies that the
project also supports C++20:

```sh
cmake -S . -B build-20 -DPROJECT_TOPIC=VIGENERE -DPROJECT_CXX_STANDARD=20
cmake --build build-20
ctest --test-dir build-20 --output-on-failure
```

## 2. Linting and formatting — clang-tidy + clang-format

Static analysis (install LLVM first: `brew install llvm`, which provides
`clang-tidy`):

```sh
SDK="$(xcrun --show-sdk-path)"
clang-tidy -p build vigenere/src/*.cpp vigenere/src/*.hpp \
  --extra-arg=-isysroot --extra-arg="$SDK" \
  --extra-arg=-isystem --extra-arg="$SDK/usr/include/c++/v1"
```

Enabled check groups are in `.clang-tidy` (`bugprone`, `performance`,
`modernize`, `readability`, `cppcoreguidelines`, with a few noisy checks
disabled). The build also compiles with `-Wall -Wextra -Wpedantic`, which is
the zeroth-cost linter and currently passes with no warnings.

Formatting (`brew install clang-format`):

```sh
clang-format --dry-run --Werror vigenere/src/*.cpp vigenere/src/*.hpp \
  vigenere/{tests/test_vigenere.cpp,bench/bench_vigenere.cpp}  # check
clang-format -i vigenere/src/*.cpp vigenere/src/*.hpp \
  vigenere/{tests/test_vigenere.cpp,bench/bench_vigenere.cpp}  # apply
```

Rules are in `.clang-format` (LLVM base, 4-space indent, 100-column limit).

## 3. Performance analysis

### Microbenchmark (function scaling)

```sh
cmake --build build --target vigenere_bench
./build/vigenere/vigenere_bench
```

Runs `find_length_kasiski`, `find_length_friedman` and `frequency_analysis` over
synthetic ciphertext from 1 k to 320 k letters. Times grow linearly with the
text size — confirming the O(n) Kasiski (direct-address trigram lookup) and the
O(K·n) Friedman, while `frequency_analysis` stays cheap.

Example (Apple Silicon, -O2):

| n       | Kasiski | Friedman | FreqAnalysis |
|---------|---------|----------|--------------|
| 1 000   | 0.003 ms | 0.006 ms | 0.005 ms |
| 20 000  | 0.080 ms | 0.081 ms | 0.013 ms |
| 320 000 | 2.14 ms  | 1.26 ms  | 0.14 ms  |

### Whole-program timing (`brew install hyperfine`)

```sh
hyperfine --warmup 2 './build/vigenere/vigenere en vigenere/data/long-encrypted.txt'
```

End-to-end this is ~30 ms, **dominated by reading the ~3 MB wordlist**, not by
the cracking maths above. The frequency cache (`frequencies.txt`, written on
first run) removes the O(dictionary) frequency computation, but the word list
is still read for the dictionary-word tie-breaker, so wall-clock time is
similar; the remaining win would require loading the word list lazily.

### Profiling (where the time actually goes)

macOS ships Instruments (Time Profiler template) with Xcode; build with
`-O2 -g` and profile the `vigenere` binary. A lighter alternative is
`samply` (`brew install samply`): `samply record ./build/vigenere/vigenere en
vigenere/data/long-encrypted.txt`, which opens the Firefox Profiler UI.
