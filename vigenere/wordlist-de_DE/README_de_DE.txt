de_DE wordlist (plain text, one word per line, UTF-8)

Source: https://github.com/davidak/wortliste
Aggregated German wordlist (~240k words) based on sources such as the
igerman98 dictionary (the basis of the de_DE Hunspell dictionary).
Contains inflected forms, umlauts and eszett as UTF-8.

unigram_freq.txt (word<TAB>count, one entry per line, UTF-8, uppercase)

Source: https://github.com/hermitdave/FrequencyWords
(content/2018/de/de_50k.txt, CC-BY-SA 4.0)
The 50k most frequent German words with their counts, from the
OpenSubtitles 2018 corpus. Converted for this project: uppercased
(including ä->Ä, ö->Ö, ü->Ü; ß kept as is) and entries containing
characters outside the cipher alphabet dropped (49,339 of 50,000 kept).
Used as the unigram cost model when re-inserting word boundaries into
spaceless decrypted text.
