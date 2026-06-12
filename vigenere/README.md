# Vigenère Cipher Encrypter / Decrypter

A C++ implementation of the Vigenère cipher that can **break encrypted text
without knowing the key**: it recovers the key length with the Kasiski
examination, recovers the key itself with frequency analysis (Kerckhoffs'
method), and then decrypts the message. Brute force is not used anywhere.

It supports two languages:

| Language | Alphabet | Arithmetic | Dictionary |
|----------|----------|-----------|------------|
| English (`en`) | A–Z (26 symbols) | mod 26 | `wordlist-en_US/en_US.txt` |
| German (`de`) | A–Z + Ä Ö Ü ß (30 symbols) | mod 30 | `wordlist-de_DE/de_DE.txt` |

In the German mode the umlauts and the eszett are **first-class cipher
symbols**: they are encrypted, decrypted and may appear in the key
(e.g. the key `MÄRCHEN`), rather than being transliterated to AE/OE/UE/SS.

## Building and running

```sh
g++ -std=c++23 vigenere/main.cpp -o vigenere_app
./vigenere_app [en|de] [encrypted-file]
```

Run it from the repository root — all file paths are relative to it.
Both arguments are optional; the defaults are `en` and
`vigenere/long-encrypted.txt`.

```sh
$ ./vigenere_app en vigenere/short-encrypted.txt
1.LOCK
2.WE MET NEXT DAY AS HE HAD ARRANGED, ...

$ ./vigenere_app de vigenere/german-encrypted.txt
1.MÄRCHEN
2.ES WAR EINMAL EIN MÜLLER, DER WAR ARM, ...
```

Output line `1.` is the recovered key, line `2.` is the decrypted message.
Spaces, punctuation and line structure of the input pass through unchanged.

The language argument must match the cipher: the English samples were
encrypted mod 26 and cannot be decrypted with mod-30 arithmetic, and vice
versa.

## Project files

| File | Description |
|------|-------------|
| `main.cpp` | The whole implementation (`Alphabet`, `VignereCypher`, `main`) |
| `short/medium/long-encrypted.txt` | English test ciphertexts (keys: LOCK, RED, FAIRY) |
| `german-encrypted.txt` | German test ciphertext (Grimm, key: MÄRCHEN) |
| `wordlist-en_US/en_US.txt` | English dictionary, ~110k words, uppercase |
| `wordlist-de_DE/de_DE.txt` | German dictionary, ~240k words, uppercase ([source](https://github.com/davidak/wortliste)) |

Both dictionaries are plain text, one word per line, fully uppercase
(with ß kept as ß, since it is its own cipher symbol). They are not used
to look up words — only to *derive the letter-frequency distribution* of
the language, so the frequencies do not need to be hardcoded.

## How the cracking works

Pipeline of `VignereCypher::Crack`:

```
ciphertext ──> ExtractLetters ──> FindLength (Kasiski) ──┐
                                                          ├──> FrequentAnalysis ──> key
dictionary ──> LetterFrequencies ─────────────────────────┘
```

### 1. Symbol extraction (`Alphabet`, `ExtractLetters`)

The cipher text is UTF-8: A–Z are one byte, Ä/Ö/Ü/ß are two bytes. The
`Alphabet` class converts between the text and an internal representation
in which **every symbol is exactly one `char`**: symbol number *i* is
stored as `'A' + i`, so Ä is the char after `'Z'`, and so on. Everything
downstream (Kasiski, frequency counting, shifting) operates on plain
`std::string` of these internal chars and never needs to know about UTF-8.

- `Alphabet::SymbolToChar(text, i)` reads the symbol at byte position `i`,
  advances `i` past it (1 or 2 bytes) and returns the internal char, or 0
  if the text does not start with an alphabet symbol there.
- `Alphabet::CharToSymbol(ch)` converts an internal char back to its text
  form.
- `Alphabet::English()` / `Alphabet::German()` are factory functions for
  the two supported alphabets.

`ExtractLetters` applies this to a whole text and keeps only the cipher
symbols — punctuation and spaces are irrelevant for the statistics.

### 2. Key length — Kasiski examination (`FindLength`)

If the same three plaintext letters are encrypted at two positions that
use the same part of the key, the ciphertext repeats too — so the
distance between repeated ciphertext trigrams is a multiple of the key
length. `FindLength` collects the distances of all repeated trigrams.

Some repeats are coincidences, so the distances are *scored* rather than
GCD-ed: candidate length `k` gets the score
`(number of distances divisible by k) × k`. A random distance is
divisible by `k` with probability 1/k, so noise contributes the same
expected amount to every candidate, while the true key length stands
out. Multiples of the true length tie with it, therefore the smallest
candidate within 90% of the best score is chosen. Supported key lengths
are 1 (a plain Caesar cipher) through `MAX_KEY_LENGTH` (10).

### 3. Key letters — frequency analysis (`FrequentAnalysis`)

With the key length `k` known, every k-th letter of the ciphertext was
shifted by the *same* key letter — each of the `k` groups is a simple
Caesar cipher (this per-group attack is Kerckhoffs' method). For each
group, all possible shifts are tried, and the shift whose resulting
letter distribution best matches the language's distribution is chosen,
using the chi-squared statistic:

```
chi² = Σ (observed_c − expected_c)² / expected_c      over all symbols c
```

The reference distribution (`LetterFrequencies`) is counted from the
dictionary instead of being hardcoded — this is what makes the same code
work for both languages. A shift that would produce a symbol the
language never uses (frequency 0) is heavily penalised.

### 4. Decryption (`decode`)

`decode` walks the original message byte by byte: alphabet symbols are
shifted back by the key (mod alphabet size) and re-emitted in their text
form; everything else is copied through unchanged. `encode` is the exact
mirror image. Note that a shift can change the byte length of a symbol
(T shifted by Ä gives ß), which is why the output string is rebuilt
rather than modified in place.

## Class reference

### `Alphabet`

| Member | Description |
|--------|-------------|
| `size()` | Number of symbols (26 or 30) |
| `SymbolToChar(text, i)` | Text → internal char at byte position `i` (advances `i`) |
| `CharToSymbol(ch)` | Internal char → text (UTF-8) form |
| `English()`, `German()` | Factory functions for the two alphabets |

### `VignereCypher`

| Member | Description |
|--------|-------------|
| `VignereCypher(key, alphabet)` | Cypher with a known key (throws if the key has no alphabet symbols) |
| `Crack(encoded, words, alphabet)` | *Static.* Recovers the key from a ciphertext and returns a ready cypher |
| `GetKey()` | The key in text form |
| `encode(message)` | Encrypts with the cypher's key |
| `encode(message, other_key)` | Encrypts with a different key, same alphabet |
| `decode(message)` | Decrypts with the cypher's key |

## Conventions and limitations

- **Everything is uppercase.** The dictionaries are stored uppercase, the
  sample ciphertexts are uppercase, and the cipher only recognises
  uppercase symbols — lowercase letters in the input pass through
  unencrypted, like punctuation. Uppercase your plaintext before
  encrypting.
- **ß stays ß.** It is never expanded to SS; it is symbol #29 of the
  German alphabet. (Its "uppercase" form in the data files is ß itself.)
- **Key length ≤ 10** (`MAX_KEY_LENGTH`). Raising the constant is enough
  to support longer keys, as long as the ciphertext is long enough.
- **The attack is statistical.** It needs a reasonable amount of
  ciphertext (a few hundred letters at minimum) — on very short texts the
  trigram repeats and the per-group letter counts become too noisy.
- **Dictionary frequencies are an approximation** of running-text
  frequencies: every word is counted once, not weighted by usage, so e.g.
  E is slightly less dominant than in real prose. In practice the
  chi-squared fit is robust to this.
