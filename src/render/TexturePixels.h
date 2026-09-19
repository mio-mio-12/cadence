#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
namespace render::texture {
inline void downscaleRgba(std::vector<std::uint8_t>& rgba, int& width, int& height, int maxDim = 1024) {
    if (width <= 0 || height <= 0 || rgba.empty()) return;
    while (width > maxDim || height > maxDim) {
        const int newW = std::max(1, width / 2);
        const int newH = std::max(1, height / 2);
        std::vector<std::uint8_t> down(static_cast<std::size_t>(newW) * newH * 4);
        for (int y = 0; y < newH; ++y) {
            for (int x = 0; x < newW; ++x) {
                const int srcX0 = x * 2, srcX1 = std::min(width - 1, srcX0 + 1);
                const int srcY0 = y * 2, srcY1 = std::min(height - 1, srcY0 + 1);
                const std::size_t i00 = (srcY0 * width + srcX0) * 4;
                const std::size_t i01 = (srcY0 * width + srcX1) * 4;
                const std::size_t i10 = (srcY1 * width + srcX0) * 4;
                const std::size_t i11 = (srcY1 * width + srcX1) * 4;
                const std::size_t dst = (y * newW + x) * 4;
                for (int c = 0; c < 4; ++c) {
                    down[dst + c] = static_cast<std::uint8_t>(
                        (static_cast<int>(rgba[i00 + c]) + rgba[i01 + c] + rgba[i10 + c] + rgba[i11 + c] + 2) / 4
                    );
                }
            }
        }
        rgba = std::move(down);
        width = newW;
        height = newH;
    }
}

struct TgaImage {
    int width{};
    int height{};
    bool hasUsefulAlpha{};
    std::vector<std::uint8_t> rgba;
};

inline bool loadTga(const std::filesystem::path& path, TgaImage& out, int maxDim = 1024) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto size = file.tellg();
    if (size < 18) return false;
    file.seekg(0);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) return false;

    const std::uint8_t idLength = data[0];
    const std::uint8_t colorMapType = data[1];
    const std::uint8_t imageType = data[2];
    const std::uint16_t width = static_cast<std::uint16_t>(data[12]) | (static_cast<std::uint16_t>(data[13]) << 8);
    const std::uint16_t height = static_cast<std::uint16_t>(data[14]) | (static_cast<std::uint16_t>(data[15]) << 8);
    const std::uint8_t pixelDepth = data[16];
    const std::uint8_t descriptor = data[17];

    if (width == 0 || height == 0) return false;
    if (colorMapType != 0) return false;

    const bool rle = (imageType == 10 || imageType == 11);
    const bool trueColor = (imageType == 2 || imageType == 10);
    const bool greyscale = (imageType == 3 || imageType == 11);
    if (!trueColor && !greyscale) return false;

    const int bpp = pixelDepth / 8;
    if (trueColor && bpp != 3 && bpp != 4) return false;
    if (greyscale && bpp != 1) return false;

    std::size_t cursor = 18 + idLength;
    const std::size_t totalPixels = static_cast<std::size_t>(width) * height;
    out.width = width;
    out.height = height;
    out.rgba.resize(totalPixels * 4);

    std::size_t pixelIndex = 0;
    std::uint8_t minAlpha = 255, maxAlpha = 0;
    std::size_t nonOpaque = 0;

    while (pixelIndex < totalPixels && cursor < data.size()) {
        if (rle) {
            const std::uint8_t header = data[cursor++];
            const std::size_t count = (header & 0x7F) + 1;
            if (header & 0x80) {
                if (cursor + bpp > data.size()) break;
                std::uint8_t b = 0, g = 0, r = 0, a = 255;
                if (bpp >= 3) {
                    b = data[cursor];
                    g = data[cursor + 1];
                    r = data[cursor + 2];
                    if (bpp == 4) a = data[cursor + 3];
                } else {
                    b = g = r = data[cursor];
                }
                cursor += bpp;

                for (std::size_t i = 0; i < count && pixelIndex < totalPixels; ++i, ++pixelIndex) {
                    out.rgba[pixelIndex * 4 + 0] = r;
                    out.rgba[pixelIndex * 4 + 1] = g;
                    out.rgba[pixelIndex * 4 + 2] = b;
                    out.rgba[pixelIndex * 4 + 3] = a;
                    minAlpha = std::min(minAlpha, a);
                    maxAlpha = std::max(maxAlpha, a);
                    if (a < 250) nonOpaque++;
                }
            } else {
                for (std::size_t i = 0; i < count && pixelIndex < totalPixels; ++i, ++pixelIndex) {
                    if (cursor + bpp > data.size()) break;
                    std::uint8_t b = 0, g = 0, r = 0, a = 255;
                    if (bpp >= 3) {
                        b = data[cursor];
                        g = data[cursor + 1];
                        r = data[cursor + 2];
                        if (bpp == 4) a = data[cursor + 3];
                    } else {
                        b = g = r = data[cursor];
                    }
                    cursor += bpp;

                    out.rgba[pixelIndex * 4 + 0] = r;
                    out.rgba[pixelIndex * 4 + 1] = g;
                    out.rgba[pixelIndex * 4 + 2] = b;
                    out.rgba[pixelIndex * 4 + 3] = a;
                    minAlpha = std::min(minAlpha, a);
                    maxAlpha = std::max(maxAlpha, a);
                    if (a < 250) nonOpaque++;
                }
            }
        } else {
            if (cursor + bpp > data.size()) break;
            std::uint8_t b = 0, g = 0, r = 0, a = 255;
            if (bpp >= 3) {
                b = data[cursor];
                g = data[cursor + 1];
                r = data[cursor + 2];
                if (bpp == 4) a = data[cursor + 3];
            } else {
                b = g = r = data[cursor];
            }
            cursor += bpp;

            out.rgba[pixelIndex * 4 + 0] = r;
            out.rgba[pixelIndex * 4 + 1] = g;
            out.rgba[pixelIndex * 4 + 2] = b;
            out.rgba[pixelIndex * 4 + 3] = a;
            minAlpha = std::min(minAlpha, a);
            maxAlpha = std::max(maxAlpha, a);
            if (a < 250) nonOpaque++;
            pixelIndex++;
        }
    }

    const bool originTopLeft = (descriptor & 0x20) != 0;
    if (originTopLeft) {
        const std::size_t rowStride = static_cast<std::size_t>(width) * 4;
        std::vector<std::uint8_t> rowBuffer(rowStride);
        for (int y = 0; y < height / 2; ++y) {
            auto* topRow = &out.rgba[static_cast<std::size_t>(y) * rowStride];
            auto* bottomRow = &out.rgba[static_cast<std::size_t>(height - 1 - y) * rowStride];
            std::memcpy(rowBuffer.data(), topRow, rowStride);
            std::memcpy(topRow, bottomRow, rowStride);
            std::memcpy(bottomRow, rowBuffer.data(), rowStride);
        }
    }

    out.hasUsefulAlpha = (maxAlpha - minAlpha >= 16) && (nonOpaque > totalPixels / 1000);
    downscaleRgba(out.rgba, out.width, out.height, maxDim);
    return true;
}
}
