// Measures how the cracking algorithms scale with synthetic ciphertext.

#include "vigenere.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

using Clock = std::chrono::steady_clock;

template <typename F> static double time_ms(F&& f, int repeats) {
    auto start = Clock::now();
    for (int r = 0; r < repeats; ++r) {
        f();
    }
    auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count() / repeats;
}

class VigenereBenchmarkAccess {
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

int main() {
    Alphabet en = Alphabet::english();

    // English-like reference distribution so the synthetic text isn't uniform.
    std::vector<double> freq = {0.082, 0.015, 0.028, 0.043, 0.127, 0.022, 0.020, 0.061, 0.070,
                                0.002, 0.008, 0.040, 0.024, 0.067, 0.075, 0.019, 0.001, 0.060,
                                0.063, 0.091, 0.028, 0.010, 0.024, 0.002, 0.020, 0.001};
    std::discrete_distribution<int> letter(freq.begin(), freq.end());
    std::mt19937 rng(1234);

    std::cout << std::left << std::setw(12) << "n" << std::setw(14) << "Kasiski(ms)"
              << std::setw(14) << "Friedman(ms)" << std::setw(16) << "FreqAnalysis(ms)" << "\n";

    for (int n : {1000, 5000, 20000, 80000, 320000}) {
        std::string text(n, 'A');
        for (char& c : text) {
            c = static_cast<char>('A' + letter(rng));
        }

        std::string cipher = VigenereCipher("SECRETKEY", en).encode(text);
        std::string letters = en.extract_letters(cipher);

        int repeats = std::max(1, 2000000 / n);
        double t_kas = time_ms(
            [&] { VigenereBenchmarkAccess::find_length_kasiski(letters, en.size()); }, repeats);
        double t_fri =
            time_ms([&] { VigenereBenchmarkAccess::find_length_friedman(letters, freq); }, repeats);
        double t_fa = time_ms(
            [&] { VigenereBenchmarkAccess::frequency_analysis(letters, 9, freq); }, repeats);

        std::cout << std::left << std::setw(12) << n << std::setw(14) << std::fixed
                  << std::setprecision(4) << t_kas << std::setw(14) << t_fri << std::setw(16)
                  << t_fa << "\n";
    }
    return 0;
}
