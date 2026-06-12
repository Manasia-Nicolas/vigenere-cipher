# Vigenère Cipher Encrypter / Decrypter

A C++ implementation of the Vigenère cipher that can encrypt, decrypt, and
recover an unknown key. It estimates the key length with the Kasiski examination
and Friedman test, then recovers the key symbols with frequency analysis
(Kerckhoffs' method). C++23 is the default; C++20 is also supported.

The project supports keys up to 10 symbols and two languages:

| Language | Alphabet | Arithmetic | Dictionary |
|----------|----------|------------|------------|
| English (`en`) | A-Z (26 symbols) | mod 26 | `wordlist-en_US/en_US.txt` |
| German (`de`) | A-Z + Ä Ö Ü ß (30 symbols) | mod 30 | `wordlist-de_DE/de_DE.txt` |

German umlauts and the eszett are first-class cipher symbols. They can appear in
the plaintext, ciphertext, and key.

## Building and Running

Run commands from the repository root:

```sh
cmake -S . -B build -DPROJECT_TOPIC=VIGENERE
cmake --build build
./build/vigenere/vigenere [en|de] [encrypted-file]
```

Both arguments are optional. The defaults are `en` and
`vigenere/data/long-encrypted.txt`.

```text
1.LOCK
2.WE MET NEXT DAY AS HE HAD ARRANGED, ...
```

Line `1.` is the recovered key and line `2.` is the decrypted message. If the
original spaces were removed before encryption, line `3.` contains the
plaintext with recovered word boundaries.

## Project Structure

| Path | Description |
|------|-------------|
| `src/main.cpp` | Command-line entry point |
| `src/alphabet.hpp` | Language symbols, UTF-8 conversion, and frequencies |
| `src/vigenere.hpp` | Encryption, decryption, and key recovery |
| `src/segmenter.hpp` | Word-boundary recovery |
| `data/` | Example encrypted files |
| `tests/` | Unit and integration tests |
| `bench/` | Performance benchmark |
| `wordlist-en_US/`, `wordlist-de_DE/` | Language dictionaries and frequency data |

## Classes

### 1. `Alphabet`

[`alphabet.hpp`](src/alphabet.hpp) stores everything that depends on the
language: its symbols, alphabet size, UTF-8 conversion rules, and letter
frequencies.

Internally, every symbol is represented by one `char` using `'A' + index`.
Therefore, German symbols such as `Ä` and `ß` occupy one internal value even
though their UTF-8 text representations use multiple bytes.

#### Variables

| Variable | Description |
|----------|-------------|
| `extras` | UTF-8 symbols after A-Z. German stores `Ä`, `Ö`, `Ü`, and `ß`; English stores none. |
| `frequencies` | Probability distribution of the language symbols, loaded from a cache or calculated from the dictionary. |
| `dictionary` | Set of valid words used to score candidate decryptions and restore word boundaries. |
| `longest_word` | Byte length of the longest dictionary word, used to detect text whose spaces were stripped. |
| `unigram_file` | Path to the language's `word count` frequency list used by `WordSegmenter`. |

#### Methods

| Method | Description |
|--------|-------------|
| `English()` | Creates a lightweight English alphabet without loading frequencies. |
| `German()` | Creates a lightweight German alphabet without loading frequencies. |
| `English(words, cache_file, unigram_file)` | Creates a resource-aware English alphabet with a dictionary, cached letter frequencies, and unigram file. |
| `German(words, cache_file, unigram_file)` | Creates a resource-aware German alphabet with a dictionary, cached letter frequencies, and unigram file. |
| `size()` | Returns the number of symbols: 26 for English or 30 for German. |
| `SymbolToChar(text, i)` | Reads one alphabet symbol from UTF-8 text, advances `i`, and returns its internal `char`. Returns `0` for a non-alphabet byte. |
| `CharToSymbol(ch)` | Converts an internal `char` back to its UTF-8 text representation. |
| `SymbolsToText(internal_symbols)` | Converts a complete internal symbol string back to UTF-8 text. |
| `ExtractLetters(text)` | Removes spaces, punctuation, digits, and symbols unsupported by this alphabet, leaving only internal alphabet symbols. |
| `LetterFrequencies(words)` | Calculates the probability of every alphabet symbol from the supplied dictionary. |
| `Frequencies()` | Returns the frequencies currently stored by the alphabet. |
| `Dictionary()` | Returns the language dictionary. |
| `UnigramFile()` | Returns the path to the language's unigram frequency list. |
| `CountDictionaryWords(text)` | Scores how much of a candidate plaintext is covered by dictionary words, including spaceless text. |
| `LoadFrequencies(cache_file, words)` | Private helper that loads a valid cache or recalculates and rewrites it from the dictionary. |

#### Method Details

##### Construction and language resources

`English()` and `German()` create lightweight alphabets that know only their
symbols. They are sufficient for encryption, decryption, and direct calls to
the statistical helper methods.

The overloads that receive `words` create resource-aware alphabets. During
construction they:

1. Store the supplied words in an `unordered_set` for fast dictionary lookup.
2. Record the longest dictionary word, used to recognize spaceless plaintext.
3. Store the path to the language's unigram file for `WordSegmenter`.
4. Load or calculate the language's letter-frequency distribution.

`VigenereCypher::Crack` and the one-shot `WordSegmenter::Segment` require these
resource-aware alphabets.

##### `SymbolToChar(text, i)` and `CharToSymbol(ch)`

The rest of the program needs every language symbol to occupy one value, even
though UTF-8 symbols may occupy multiple bytes. `SymbolToChar` provides this
conversion while advancing `i` past the bytes it consumed:

```text
text symbol:       A    Z    Ä    Ö    Ü    ß
internal value:   'A'  'Z'  '['  '\'  ']'  '^'
```

For A-Z, the text byte is already the internal value. For a German extra
symbol, the method finds its position in `extras` and returns
`'A' + 26 + position`. If no supported symbol starts at `i`, it advances by one
byte and returns `0`.

`CharToSymbol` performs the inverse conversion. `SymbolsToText` applies it to a
complete internal string.

##### `ExtractLetters(text)`

`ExtractLetters` repeatedly calls `SymbolToChar` and keeps only nonzero results.
It therefore converts UTF-8 text into the internal one-value-per-symbol format
while removing spaces, punctuation, digits, lowercase letters, and unsupported
characters.

For example, with the German alphabet:

```text
"KÖNIG, 123!" -> internal symbols for "KÖNIG"
```

The cracking algorithms operate on this compact internal representation.

##### `LetterFrequencies(words)` and `LoadFrequencies(cache_file, words)`

`LetterFrequencies` counts every supported symbol in the supplied dictionary
and divides each count by the total:

```text
frequency(symbol) = occurrences(symbol) / total_supported_symbols
```

This produces the language distribution used by the Friedman test and
frequency analysis.

Calculating it requires scanning the entire dictionary, so `LoadFrequencies`
first tries the cache file. A cache is accepted only when it contains exactly
one value per alphabet symbol and those values sum to approximately `1`. An
invalid or missing cache is recalculated from the dictionary and rewritten.

##### `CountDictionaryWords(text)`

This method scores how readable a candidate decryption is:

1. It separates normal text at unsupported characters such as spaces and
   punctuation, then counts exact dictionary words.
2. It tracks the longest uninterrupted alphabet-symbol run.
3. If no words were counted and a run is longer than any dictionary word, it
   assumes the spaces were stripped.
4. It greedily finds the longest dictionary word at each position.
5. It adds `length²` for each match, strongly preferring long real words over
   accidental sequences of short words.

`VigenereCypher::Crack` uses this score when Kasiski and Friedman suggest
different key lengths.

The main `Alphabet` flow is:

```text
UTF-8 text -> SymbolToChar / ExtractLetters -> internal symbols
dictionary -> LetterFrequencies / LoadFrequencies -> language distribution
candidate plaintext -> CountDictionaryWords -> readability score
```

### 2. `VigenereCypher`

[`vigenere.hpp`](src/vigenere.hpp) encrypts and decrypts text with a known key.
It can also recover an unknown key from ciphertext.

#### Variables

| Variable | Description |
|----------|-------------|
| `alphabet` | Defines the symbols and arithmetic used by the cipher. |
| `key` | The key stored in the alphabet's one-`char`-per-symbol internal format. |
| `MAX_KEY_LENGTH` | Largest key length considered by the cracking algorithms. Currently `10`. |

#### Methods

| Method | Description |
|--------|-------------|
| `VigenereCypher(key, alphabet)` | Creates a cipher with a known key. Throws if the key contains no supported symbols. |
| `encode(message)` | Encrypts supported symbols with the stored key while preserving spaces and punctuation. |
| `encode(message, other_key)` | Encrypts using another key without changing the current cipher. |
| `decode(message)` | Decrypts supported symbols with the stored key while preserving other characters. |
| `GetKey()` | Returns the stored key in its UTF-8 text form. |
| `Crack(encoded, alphabet)` | Recovers the key using the resource-aware alphabet and returns a cipher ready to decrypt the message. |
| `FindLengthKasiski(encoded, n_symbols)` | Estimates key length by scoring distances between repeated ciphertext trigrams. |
| `FindLengthFriedman(encoded, language_freq)` | Estimates key length by comparing each candidate's column IC with the language-derived expected IC. |
| `FrequentAnalysis(encoded, length, language_freq)` | Treats every key column as a Caesar cipher and chooses each shift with chi-squared frequency scoring. This is the Kerckhoffs attack used by the project. |
| `helper_increase(a, b)` | Private helper that shifts one symbol forward during encryption. |
| `helper_decrease(a, b)` | Private helper that shifts one symbol backward during decryption. |

#### Method Details

##### `VigenereCypher(key, alphabet)`

The constructor stores the selected alphabet and converts the key from UTF-8
text into the alphabet's internal one-`char`-per-symbol representation.
Unsupported characters in the key are ignored. If no valid key symbols remain,
the constructor throws an exception because encryption and decryption would be
impossible.

##### `encode(message)` and `decode(message)`

Both methods process the original message one symbol at a time. Supported
alphabet symbols are shifted using the current key, while spaces, punctuation,
digits, and unsupported characters are copied unchanged.

The key position advances only when a supported symbol is processed. This means
punctuation does not change which key symbol encrypts the next letter.

Encryption adds the key shift:

```text
encrypted = (message + key) mod alphabet_size
```

Decryption subtracts it:

```text
decrypted = (message - key + alphabet_size) mod alphabet_size
```

The output string is rebuilt instead of edited in place because German symbols
may use a different number of UTF-8 bytes after shifting.

##### `encode(message, other_key)`

This overload creates a temporary `VigenereCypher` with `other_key` and the
same alphabet, then encrypts the message with it. The key stored by the current
object is not changed.

##### `FindLengthKasiski(encoded, n_symbols)`

This method performs the Kasiski examination:

1. It finds repeated groups of three ciphertext symbols.
2. It records the distance between repeated groups.
3. For every possible key length from `1` to `MAX_KEY_LENGTH`, it counts how
   many distances are divisible by that length.
4. It multiplies the count by the candidate length so larger candidates do not
   lose only because random distances are less often divisible by them.
5. It returns the smallest candidate whose score is within 90% of the best
   score.

Repeated ciphertext groups often occur when the same plaintext was encrypted
at the same key position. Their distances are therefore commonly divisible by
the real key length.

##### `FindLengthFriedman(encoded, language_freq)`

This method estimates the key length using the index of coincidence. For every
candidate length, it splits the ciphertext into columns. Each column contains
symbols encrypted by the same position of the repeating key.

First, it derives the expected IC directly from the selected language:

```text
expected_IC = sum(probability_of_symbol²)
```

The correct candidate produces columns that behave like shifted natural
language, so their average IC should be close to this expected value. Wrong
candidates mix different shifts together and move the average IC away from the
language value.

Multiples of the real key length also create valid single-shift columns, and
their shorter columns may appear closer by chance. Therefore, the method
returns the smallest candidate whose error is within a small
language-proportional tolerance of the best error.

##### `FrequentAnalysis(encoded, length, language_freq)`

Once the key length is known, this method recovers every key symbol separately:

1. It groups together every `length`-th ciphertext symbol.
2. It treats each group as a Caesar cipher encrypted by one key symbol.
3. It tries every possible shift in the alphabet.
4. It compares each shifted distribution with the language distribution using
   the chi-squared statistic.
5. It keeps the shift with the lowest chi-squared score.

Combining the best shift from every group produces the complete key. This
per-column frequency-analysis attack is the Kerckhoffs method used by the
project.

##### `Crack(encoded, alphabet)`

`Crack` coordinates the complete key-recovery process:

1. It removes punctuation and unsupported characters with `ExtractLetters`.
2. It obtains language frequencies from the resource-aware alphabet.
3. It estimates the key length with both Kasiski and Friedman analysis.
4. It runs `FrequentAnalysis` for the estimated length.
5. If both methods agree on the length, it immediately returns the recovered
   cipher.
6. If they disagree, it creates one candidate cipher for each length, decrypts
   with both, and uses `Alphabet::CountDictionaryWords` to return the candidate
   with better dictionary coverage.

`Crack` requires an alphabet created with `English(words, ...)` or
`German(words, ...)`; a lightweight alphabet has no frequencies or dictionary
and causes `Crack` to throw.

##### `Alphabet::CountDictionaryWords(text)`

This method first counts complete dictionary words separated by non-alphabet
characters. If the text contains one long run with no separators, it falls back
to longest dictionary matches and rewards longer matches with `length²`. It is
used as a readability score when `Crack` must choose between candidate keys.

##### `GetKey()`

`GetKey` asks the stored alphabet to convert the internal key with
`SymbolsToText`. The conversion belongs to `Alphabet` because interpreting an
internal symbol depends on that alphabet's extra symbols.

##### `helper_increase(a, b)` and `helper_decrease(a, b)`

These private helpers implement modular symbol shifting. `helper_increase`
adds a key symbol during encryption, while `helper_decrease` subtracts it
during decryption. Both wrap around using the selected alphabet's size.

`Crack` follows this pipeline:

```text
ciphertext -> ExtractLetters -> Kasiski and Friedman -> key length
resource-aware Alphabet -> frequencies and dictionary scoring
language frequencies + key length -> FrequentAnalysis -> key
key + ciphertext -> decode -> plaintext
```

### 3. `WordSegmenter`

[`segmenter.hpp`](src/segmenter.hpp) restores spaces in decrypted text such as
`THEQUICKBROWNFOX`. It uses dynamic programming to find the lowest-cost sequence
of words.

English and German use their own unigram frequencies, so common words receive
lower costs. If a frequency file is unavailable, the segmenter uses a
dictionary-only fallback that prefers longer valid words.

#### Variables

| Variable | Description |
|----------|-------------|
| `word_cost` | Maps every known word to its segmentation cost. Lower costs are preferred. |
| `unknown_cost` | Cost of preserving one unknown symbol when no dictionary word covers it. |
| `max_word_length` | Longest known word in this segmenter's language resources, measured in symbols rather than UTF-8 bytes. |

#### Methods

| Method | Description |
|--------|-------------|
| `Build(frequency_file, dictionary)` | Creates a segmenter using unigram costs when possible, otherwise using dictionary-only costs, and derives the language-specific maximum word length. |
| `Segment(text, alphabet)` | Instance method that splits spaceless text into the minimum-cost sequence of known words and unknown symbols. |
| `Segment(alphabet, text)` | Static convenience method that builds from the alphabet's language resources and segments in one call. |
| `NormalizeAscii(word)` | Private helper that uppercases clean ASCII words and rejects invalid frequency-list entries. |
| `SymbolLength(word)` | Private helper that counts UTF-8 symbols so German letters count as one symbol. |

#### Method Details

##### `Build(frequency_file, dictionary)`

`Build` creates the cost model that later decides which word boundaries are
most likely. It first reads valid `word count` pairs from the frequency file,
normalizes ASCII words to uppercase, and totals their counts.

When frequency data exists, each word receives a negative log-probability cost:

```text
probability(word) = count(word) / total_count
cost(word) = -log10(probability(word))
           = log10(total_count) - log10(count(word))
```

Common words have low costs and rare words have high costs. Logarithms turn the
probability of a complete word sequence from multiplication into addition:

```text
P(THE CAT) = P(THE) * P(CAT)
cost(THE CAT) = cost(THE) + cost(CAT)
```

Dictionary words missing from the frequency file are still accepted, but
receive the cost of a word seen once. Unknown single symbols receive an even
higher cost so they are used only when no real word covers that position.

If no valid frequency data exists, `Build` uses the dictionary-only model:

```text
cost(word) = -(symbol_length²)
```

The negative value rewards longer dictionary matches. In both models, `Build`
also records the longest known word in symbols. This gives English and German
their own search limits and ensures UTF-8 letters such as `Ä` count as one
symbol.

##### `Segment(text, alphabet)`

`Segment` uses dynamic programming to find the valid segmentation with the
lowest total cost.

First, it tokenizes the input through `Alphabet`:

```text
"KÖNIGGRÜßT" -> ["K", "Ö", "N", "I", "G", "G", "R", "Ü", "ß", "T"]
```

It then creates two arrays with one entry for every text position:

| Array | Meaning |
|-------|---------|
| `best[i]` | Lowest cost found for segmenting the first `i` symbols. |
| `prev[i]` | Position where the final word ending at `i` begins. |

`best[0]` is `0` because segmenting empty text costs nothing. Every other entry
starts at infinity.

For each ending position, the algorithm grows possible words leftwards, up to
`max_word_length`. A candidate is accepted when it is a known word, or when it
is one unknown symbol. Its complete score is:

```text
score = best[start] + cost(candidate_word)
```

If this score is lower than `best[end]`, the algorithm stores it and records
`start` in `prev[end]`.

For example, after considering `"THECAT"`:

```text
best[3] = cost("THE")
best[6] = best[3] + cost("CAT")
prev[6] = 3
```

After all positions are processed, `Segment` follows `prev` backwards from the
end to recover the chosen words. Because this produces words in reverse order,
it traverses them backwards once more while joining them with spaces.

##### `Segment(alphabet, text)`

The static overload is a convenience wrapper. It obtains the dictionary and
unigram path stored by the resource-aware `Alphabet`, builds a temporary
segmenter, and immediately segments the text:

```text
Alphabet resources -> Build -> dynamic programming -> reconstructed words
```

The overall segmentation flow is:

```text
frequency list + dictionary -> Build -> word costs and maximum word length
spaceless UTF-8 text -> Alphabet tokenization -> dynamic programming
best/prev arrays -> reconstruction -> text with spaces
```

## Testing

The suite contains 30 Doctest cases plus five CTest entries. It covers alphabet
conversion, English and German encryption, known answers, key recovery, sample
files, frequency caches, CLI behavior, and language-specific segmentation
limits.

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

Run with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake -S . -B build-san -DPROJECT_TOPIC=VIGENERE -DVIGENERE_SANITIZE=ON
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

## Benchmark and Linting

```sh
cmake --build build --target vigenere_bench
./build/vigenere/vigenere_bench
```

On macOS, Homebrew installs LLVM as keg-only. Install it and add its tools to
the shell path:

```sh
brew install llvm
echo 'export PATH="/opt/homebrew/opt/llvm/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

Homebrew LLVM may also need the active Xcode SDK paths to find the macOS C++
standard-library headers:

```sh
SDK="$(xcrun --show-sdk-path)"
clang-tidy -p build vigenere/src/main.cpp vigenere/src/*.hpp \
  --extra-arg=-isysroot --extra-arg="$SDK" \
  --extra-arg=-isystem --extra-arg="$SDK/usr/include/c++/v1"

clang-format --dry-run --Werror vigenere/src/*.cpp vigenere/src/*.hpp \
  vigenere/{tests/test_vigenere.cpp,bench/bench_vigenere.cpp}
```

## Limitations

- Input should be uppercase. Lowercase letters are preserved but not encrypted.
- Supported key lengths are limited to 10.
- Cracking is statistical and requires enough ciphertext for reliable results.
- The selected language must match the alphabet used during encryption.
