#include <BitMatrix.h>
#include <HybridBinarizer.h>
#include <ZXingCpp.h>
#include <qrcode/QRBitMatrixParser.h>

#include <bitset>
#include <cstdint>
#include <iostream>
#include <print>
#include <ranges>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace ZXing;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: qrcode <image>\n";
        return 1;
    }
    std::string filePath = argv[1];
    int width, height, channels;

    std::unique_ptr<stbi_uc, void (*)(void*)> buffer(
        stbi_load(filePath.c_str(), &width, &height, &channels, 0),
        stbi_image_free);
    auto ImageFormatFromChannels =
        std::array{ImageFormat::None, ImageFormat::Lum, ImageFormat::LumA,
                   ImageFormat::RGB, ImageFormat::RGBA};
    ImageView image{buffer.get(), width, height,
                    ImageFormatFromChannels.at(channels)};

    // Crop size is always 4 in the test cases.
    // Change the version if you are not reading Version 1.
    int version = 1;
    auto size = 17 + 4 * version;
    auto cropped = image.cropped(4, 4, size, size);

    // bitmap is a matrix of bool (true: black, false: white).
    auto bitmap = std::make_unique<HybridBinarizer>(cropped)->getBlackMatrix();

    // Draw bitmap to show that it is exactly the QR Code we read in.
    // You can delete this code.
    std::println("bitmap:");
    for (auto i : std::views::iota(0, 21)) {
        for (auto j : std::views::iota(0, 21)) {
            bitmap->get(j, i) == true ? std::print("■ ") : std::print("  ");
        }
        std::println("");
    }

    // You decoder goes here.

    // You can compare your results using the solution below.
    // ReaderOptions options;
    // options.tryHarder(false)
    //     .tryRotate(false)
    //     .tryInvert(false)
    //     .tryDownscale(false)
    //     .maxNumberOfSymbols(1)
    //     .isPure(true)
    //     .returnErrors(true);
    // auto barcodes = ReadBarcodes(image, options);
    // std::println("{}", barcodes[0].text());
}
