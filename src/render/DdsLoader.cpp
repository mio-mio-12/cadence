#include "render/DdsLoader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace render::dds {

namespace {

constexpr std::uint32_t kDdsMagic = 0x20534444; // "DDS "
constexpr std::uint32_t kFourCcDx10 = 0x30315844; // "DX10"
constexpr std::uint32_t kFourCcDxt1 = 0x31545844; // "DXT1"
constexpr std::uint32_t kFourCcDxt3 = 0x33545844; // "DXT3"
constexpr std::uint32_t kFourCcDxt5 = 0x35545844; // "DXT5"
constexpr std::uint32_t kFourCcAti1 = 0x31495441; // "ATI1"
constexpr std::uint32_t kFourCcBc4u = 0x55344342; // "BC4U"
constexpr std::uint32_t kFourCcBc4s = 0x53344342; // "BC4S"
constexpr std::uint32_t kFourCcAti2 = 0x32495441; // "ATI2"
constexpr std::uint32_t kFourCcBc5u = 0x55354342; // "BC5U"
constexpr std::uint32_t kFourCcBc5s = 0x53354342; // "BC5S"
constexpr std::uint32_t kFourCcDxn  = 0x204E5844; // "DXN "

// OpenGL Format Constants
constexpr unsigned int kGlRgba8 = 0x8058;
constexpr unsigned int kGlR8 = 0x8229;
constexpr unsigned int kGlRg8 = 0x822B;
constexpr unsigned int kGlCompressedRgbaDxt1 = 0x83F1;
constexpr unsigned int kGlCompressedRgbaDxt3 = 0x83F2;
constexpr unsigned int kGlCompressedRgbaDxt5 = 0x83F3;
constexpr unsigned int kGlCompressedRedRgtc1 = 0x8DBB;
constexpr unsigned int kGlCompressedSignedRedRgtc1 = 0x8DBC;
constexpr unsigned int kGlCompressedRgRgtc2 = 0x8DBD;
constexpr unsigned int kGlCompressedSignedRgRgtc2 = 0x8DBE;
constexpr unsigned int kGlCompressedRgbaBptc = 0x8E8C;

// OpenGL base formats & types
constexpr unsigned int kGlRgba = 0x1908;
constexpr unsigned int kGlBgra = 0x80E1;
constexpr unsigned int kGlRgb = 0x1907;
constexpr unsigned int kGlBgr = 0x80E0;
constexpr unsigned int kGlRed = 0x1903;
constexpr unsigned int kGlRg = 0x8227;
constexpr unsigned int kGlUnsignedByte = 0x1401;

inline std::uint16_t readU16(const std::uint8_t* ptr) {
    return static_cast<std::uint16_t>(ptr[0] | (static_cast<std::uint16_t>(ptr[1]) << 8));
}

inline std::uint32_t readU32(const std::uint8_t* ptr) {
    return static_cast<std::uint32_t>(ptr[0]) |
           (static_cast<std::uint32_t>(ptr[1]) << 8) |
           (static_cast<std::uint32_t>(ptr[2]) << 16) |
           (static_cast<std::uint32_t>(ptr[3]) << 24);
}

void decode565(std::uint16_t color, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    const auto r5 = static_cast<std::uint8_t>((color >> 11) & 0x1F);
    const auto g6 = static_cast<std::uint8_t>((color >> 5) & 0x3F);
    const auto b5 = static_cast<std::uint8_t>(color & 0x1F);
    r = static_cast<std::uint8_t>((r5 * 527 + 23) >> 6);
    g = static_cast<std::uint8_t>((g6 * 259 + 33) >> 6);
    b = static_cast<std::uint8_t>((b5 * 527 + 23) >> 6);
}

} // namespace

bool parse(const std::uint8_t* bytes, std::size_t byteCount, Image& image, std::string& error) {
    error.clear();
    if (!bytes || byteCount < 128) {
        error = "Buffer is too small for a DDS header (minimum 128 bytes)";
        return false;
    }

    const std::uint32_t magic = readU32(bytes);
    if (magic != kDdsMagic) {
        error = "Invalid DDS magic signature (expected 'DDS ')";
        return false;
    }

    const std::uint32_t headerSize = readU32(bytes + 4);
    if (headerSize != 124) {
        error = "Invalid DDS header size (expected 124 bytes)";
        return false;
    }

    image.height = readU32(bytes + 12);
    image.width = readU32(bytes + 16);
    image.depth = std::max(1u, readU32(bytes + 24));
    image.mipCount = std::max(1u, readU32(bytes + 28));

    if (!image.width || !image.height) {
        error = "Invalid DDS image dimensions (0x0)";
        return false;
    }

    const std::uint32_t pfSize = readU32(bytes + 76);
    const std::uint32_t pfFlags = readU32(bytes + 80);
    const std::uint32_t fourCC = readU32(bytes + 84);
    const std::uint32_t rgbBitCount = readU32(bytes + 88);
    const std::uint32_t rMask = readU32(bytes + 92);
    const std::uint32_t gMask = readU32(bytes + 96);
    const std::uint32_t bMask = readU32(bytes + 100);
    const std::uint32_t aMask = readU32(bytes + 104);
    const std::uint32_t caps2 = readU32(bytes + 112);

    image.isCubemap = (caps2 & 0x200) != 0; // DDSCAPS2_CUBEMAP

    std::size_t offset = 128;
    image.format = Format::Unknown;
    image.isCompressed = false;
    image.blockSize = 0;

    if (pfFlags & 0x4) { // DDPF_FOURCC
        if (fourCC == kFourCcDxt1) {
            image.format = Format::BC1_RGBA;
            image.glInternalFormat = kGlCompressedRgbaDxt1;
            image.isCompressed = true;
            image.blockSize = 8;
        } else if (fourCC == kFourCcDxt3) {
            image.format = Format::BC2_RGBA;
            image.glInternalFormat = kGlCompressedRgbaDxt3;
            image.isCompressed = true;
            image.blockSize = 16;
        } else if (fourCC == kFourCcDxt5) {
            image.format = Format::BC3_RGBA;
            image.glInternalFormat = kGlCompressedRgbaDxt5;
            image.isCompressed = true;
            image.blockSize = 16;
        } else if (fourCC == kFourCcAti1 || fourCC == kFourCcBc4u) {
            image.format = Format::BC4_R_UNORM;
            image.glInternalFormat = kGlCompressedRedRgtc1;
            image.isCompressed = true;
            image.blockSize = 8;
        } else if (fourCC == kFourCcBc4s) {
            image.format = Format::BC4_R_SNORM;
            image.glInternalFormat = kGlCompressedSignedRedRgtc1;
            image.isCompressed = true;
            image.blockSize = 8;
        } else if (fourCC == kFourCcAti2 || fourCC == kFourCcBc5u || fourCC == kFourCcDxn) {
            image.format = Format::BC5_RG_UNORM;
            image.glInternalFormat = kGlCompressedRgRgtc2;
            image.isCompressed = true;
            image.blockSize = 16;
        } else if (fourCC == kFourCcBc5s) {
            image.format = Format::BC5_RG_SNORM;
            image.glInternalFormat = kGlCompressedSignedRgRgtc2;
            image.isCompressed = true;
            image.blockSize = 16;
        } else if (fourCC == kFourCcDx10) {
            if (byteCount < 148) {
                error = "DDS with DX10 header is truncated";
                return false;
            }
            offset = 148;
            const std::uint32_t dxgiFormat = readU32(bytes + 128);
            const std::uint32_t arraySize = readU32(bytes + 140);
            image.arraySize = std::max(1u, arraySize);

            switch (dxgiFormat) {
                case 71: // DXGI_FORMAT_BC1_UNORM
                case 72: // DXGI_FORMAT_BC1_UNORM_SRGB
                    image.format = Format::BC1_RGBA;
                    image.glInternalFormat = kGlCompressedRgbaDxt1;
                    image.isCompressed = true;
                    image.blockSize = 8;
                    break;
                case 74: // DXGI_FORMAT_BC2_UNORM
                case 75: // DXGI_FORMAT_BC2_UNORM_SRGB
                    image.format = Format::BC2_RGBA;
                    image.glInternalFormat = kGlCompressedRgbaDxt3;
                    image.isCompressed = true;
                    image.blockSize = 16;
                    break;
                case 77: // DXGI_FORMAT_BC3_UNORM
                case 78: // DXGI_FORMAT_BC3_UNORM_SRGB
                    image.format = Format::BC3_RGBA;
                    image.glInternalFormat = kGlCompressedRgbaDxt5;
                    image.isCompressed = true;
                    image.blockSize = 16;
                    break;
                case 80: // DXGI_FORMAT_BC4_UNORM
                    image.format = Format::BC4_R_UNORM;
                    image.glInternalFormat = kGlCompressedRedRgtc1;
                    image.isCompressed = true;
                    image.blockSize = 8;
                    break;
                case 81: // DXGI_FORMAT_BC4_SNORM
                    image.format = Format::BC4_R_SNORM;
                    image.glInternalFormat = kGlCompressedSignedRedRgtc1;
                    image.isCompressed = true;
                    image.blockSize = 8;
                    break;
                case 83: // DXGI_FORMAT_BC5_UNORM
                    image.format = Format::BC5_RG_UNORM;
                    image.glInternalFormat = kGlCompressedRgRgtc2;
                    image.isCompressed = true;
                    image.blockSize = 16;
                    break;
                case 84: // DXGI_FORMAT_BC5_SNORM
                    image.format = Format::BC5_RG_SNORM;
                    image.glInternalFormat = kGlCompressedSignedRgRgtc2;
                    image.isCompressed = true;
                    image.blockSize = 16;
                    break;
                case 98: // DXGI_FORMAT_BC7_UNORM
                case 99: // DXGI_FORMAT_BC7_UNORM_SRGB
                    image.format = Format::BC7_RGBA;
                    image.glInternalFormat = kGlCompressedRgbaBptc;
                    image.isCompressed = true;
                    image.blockSize = 16;
                    break;
                case 28: // DXGI_FORMAT_R8G8B8A8_UNORM
                case 29: // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                    image.format = Format::RGBA8;
                    image.glInternalFormat = kGlRgba8;
                    image.glFormat = kGlRgba;
                    image.glType = kGlUnsignedByte;
                    break;
                case 87: // DXGI_FORMAT_B8G8R8A8_UNORM
                case 91: // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
                    image.format = Format::BGRA8;
                    image.glInternalFormat = kGlRgba8;
                    image.glFormat = kGlBgra;
                    image.glType = kGlUnsignedByte;
                    break;
                case 61: // DXGI_FORMAT_R8_UNORM
                    image.format = Format::R8;
                    image.glInternalFormat = kGlR8;
                    image.glFormat = kGlRed;
                    image.glType = kGlUnsignedByte;
                    break;
                case 49: // DXGI_FORMAT_R8G8_UNORM
                    image.format = Format::RG8;
                    image.glInternalFormat = kGlRg8;
                    image.glFormat = kGlRg;
                    image.glType = kGlUnsignedByte;
                    break;
                default:
                    error = "Unsupported DX10 DXGI format: " + std::to_string(dxgiFormat);
                    return false;
            }
        }
    } else if (pfFlags & 0x40 || pfFlags & 0x41) { // DDPF_RGB or DDPF_RGBA
        if (rgbBitCount == 32) {
            if (rMask == 0x00FF0000 && gMask == 0x0000FF00 && bMask == 0x000000FF) {
                image.format = Format::BGRA8;
                image.glInternalFormat = kGlRgba8;
                image.glFormat = kGlBgra;
                image.glType = kGlUnsignedByte;
            } else {
                image.format = Format::RGBA8;
                image.glInternalFormat = kGlRgba8;
                image.glFormat = kGlRgba;
                image.glType = kGlUnsignedByte;
            }
        } else if (rgbBitCount == 24) {
            if (rMask == 0x00FF0000 && gMask == 0x0000FF00 && bMask == 0x000000FF) {
                image.format = Format::BGR8;
                image.glInternalFormat = 0x8051; // GL_RGB8
                image.glFormat = kGlBgr;
                image.glType = kGlUnsignedByte;
            } else {
                image.format = Format::RGB8;
                image.glInternalFormat = 0x8051;
                image.glFormat = kGlRgb;
                image.glType = kGlUnsignedByte;
            }
        } else if (rgbBitCount == 8) {
            image.format = Format::R8;
            image.glInternalFormat = kGlR8;
            image.glFormat = kGlRed;
            image.glType = kGlUnsignedByte;
        }
    }

    if (image.format == Format::Unknown) {
        error = "Unrecognized or unsupported DDS pixel format";
        return false;
    }

    if (image.isCompressed) {
        const std::size_t blocksW = (image.width + 3) / 4;
        const std::size_t blocksH = (image.height + 3) / 4;
        image.topMipByteSize = blocksW * blocksH * image.blockSize;
    } else {
        if (image.format == Format::RGBA8 || image.format == Format::BGRA8) {
            image.topMipByteSize = static_cast<std::size_t>(image.width) * image.height * 4;
        } else if (image.format == Format::RGB8 || image.format == Format::BGR8) {
            image.topMipByteSize = static_cast<std::size_t>(image.width) * image.height * 3;
        } else if (image.format == Format::R8) {
            image.topMipByteSize = static_cast<std::size_t>(image.width) * image.height;
        } else if (image.format == Format::RG8) {
            image.topMipByteSize = static_cast<std::size_t>(image.width) * image.height * 2;
        }
    }

    if (byteCount < offset + image.topMipByteSize) {
        error = "DDS file data truncated (expected at least " + std::to_string(offset + image.topMipByteSize) + " bytes, got " + std::to_string(byteCount) + ")";
        return false;
    }

    image.data.assign(bytes + offset, bytes + byteCount);
    image.hasUsefulAlpha = checkUsefulAlpha(image);

    return true;
}

bool loadFile(const std::filesystem::path& path, Image& image, std::string& error) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        error = "Could not open file: " + path.string();
        return false;
    }
    const auto size = input.tellg();
    if (size < 128) {
        error = "File is too small to be a DDS texture";
        return false;
    }
    input.seekg(0);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(buffer.data()), size);
    if (!input) {
        error = "Failed reading DDS file content";
        return false;
    }
    return parse(buffer.data(), buffer.size(), image, error);
}

bool checkUsefulAlpha(const Image& image) {
    if (image.data.empty() || !image.width || !image.height) return false;

    if (image.format == Format::BC3_RGBA || image.format == Format::BC2_RGBA) {
        // Fast inspect BC3 alpha blocks
        const std::size_t blockCount = ((image.width + 3) / 4) * ((image.height + 3) / 4);
        std::uint8_t minAlpha = 255, maxAlpha = 0;
        std::size_t nonOpaqueBlocks = 0;

        for (std::size_t b = 0; b < blockCount && b * 16 < image.topMipByteSize; ++b) {
            const auto* block = image.data.data() + b * 16;
            const auto a0 = block[0], a1 = block[1];
            minAlpha = std::min({minAlpha, a0, a1});
            maxAlpha = std::max({maxAlpha, a0, a1});
            if (a0 < 250 || a1 < 250) ++nonOpaqueBlocks;
        }
        return (maxAlpha - minAlpha >= 16) && (nonOpaqueBlocks > blockCount / 100);
    } else if (image.format == Format::RGBA8 || image.format == Format::BGRA8) {
        std::uint8_t minAlpha = 255, maxAlpha = 0;
        std::size_t nonOpaque = 0;
        const std::size_t totalPixels = static_cast<std::size_t>(image.width) * image.height;
        const auto* ptr = image.data.data();

        for (std::size_t i = 0; i < totalPixels && (i * 4 + 3) < image.topMipByteSize; i += 4) {
            const auto a = ptr[i * 4 + 3];
            minAlpha = std::min(minAlpha, a);
            maxAlpha = std::max(maxAlpha, a);
            if (a < 250) ++nonOpaque;
        }
        return (maxAlpha - minAlpha >= 16) && (nonOpaque > totalPixels / 1000);
    }
    return false;
}

bool decompressTopMipToRgba(const Image& image, std::vector<std::uint8_t>& outRgba) {
    if (image.data.empty() || !image.width || !image.height) return false;
    outRgba.assign(static_cast<std::size_t>(image.width) * image.height * 4, 255);

    const std::uint32_t width = image.width;
    const std::uint32_t height = image.height;
    const auto* src = image.data.data();

    if (image.format == Format::BC1_RGBA) {
        const std::uint32_t blocksW = (width + 3) / 4;
        const std::uint32_t blocksH = (height + 3) / 4;

        for (std::uint32_t by = 0; by < blocksH; ++by) {
            for (std::uint32_t bx = 0; bx < blocksW; ++bx) {
                const auto* block = src + (by * blocksW + bx) * 8;
                const auto c0 = readU16(block);
                const auto c1 = readU16(block + 2);
                const auto code = readU32(block + 4);

                std::uint8_t colors[4][4]{};
                decode565(c0, colors[0][0], colors[0][1], colors[0][2]); colors[0][3] = 255;
                decode565(c1, colors[1][0], colors[1][1], colors[1][2]); colors[1][3] = 255;

                if (c0 > c1) {
                    colors[2][0] = static_cast<std::uint8_t>((2 * colors[0][0] + colors[1][0]) / 3);
                    colors[2][1] = static_cast<std::uint8_t>((2 * colors[0][1] + colors[1][1]) / 3);
                    colors[2][2] = static_cast<std::uint8_t>((2 * colors[0][2] + colors[1][2]) / 3);
                    colors[2][3] = 255;

                    colors[3][0] = static_cast<std::uint8_t>((colors[0][0] + 2 * colors[1][0]) / 3);
                    colors[3][1] = static_cast<std::uint8_t>((colors[0][1] + 2 * colors[1][1]) / 3);
                    colors[3][2] = static_cast<std::uint8_t>((colors[0][2] + 2 * colors[1][2]) / 3);
                    colors[3][3] = 255;
                } else {
                    colors[2][0] = static_cast<std::uint8_t>((colors[0][0] + colors[1][0]) / 2);
                    colors[2][1] = static_cast<std::uint8_t>((colors[0][1] + colors[1][1]) / 2);
                    colors[2][2] = static_cast<std::uint8_t>((colors[0][2] + colors[1][2]) / 2);
                    colors[2][3] = 255;

                    colors[3][0] = colors[3][1] = colors[3][2] = colors[3][3] = 0;
                }

                for (std::uint32_t py = 0; py < 4; ++py) {
                    for (std::uint32_t px = 0; px < 4; ++px) {
                        const std::uint32_t x = bx * 4 + px;
                        const std::uint32_t y = by * 4 + py;
                        if (x < width && y < height) {
                            const auto idx = (code >> (2 * (py * 4 + px))) & 0x3;
                            const std::size_t outIdx = (y * width + x) * 4;
                            outRgba[outIdx + 0] = colors[idx][0];
                            outRgba[outIdx + 1] = colors[idx][1];
                            outRgba[outIdx + 2] = colors[idx][2];
                            outRgba[outIdx + 3] = colors[idx][3];
                        }
                    }
                }
            }
        }
        return true;
    } else if (image.format == Format::BC3_RGBA) {
        const std::uint32_t blocksW = (width + 3) / 4;
        const std::uint32_t blocksH = (height + 3) / 4;

        for (std::uint32_t by = 0; by < blocksH; ++by) {
            for (std::uint32_t bx = 0; bx < blocksW; ++bx) {
                const auto* block = src + (by * blocksW + bx) * 16;
                const auto a0 = block[0], a1 = block[1];
                std::uint64_t aIndices = 0;
                for (int k = 0; k < 6; ++k) aIndices |= (static_cast<std::uint64_t>(block[2 + k]) << (k * 8));

                std::uint8_t alphas[8]{};
                alphas[0] = a0; alphas[1] = a1;
                if (a0 > a1) {
                    for (int k = 1; k <= 6; ++k) alphas[k + 1] = static_cast<std::uint8_t>(((8 - k) * a0 + k * a1) / 7);
                } else {
                    for (int k = 1; k <= 4; ++k) alphas[k + 1] = static_cast<std::uint8_t>(((6 - k) * a0 + k * a1) / 5);
                    alphas[6] = 0; alphas[7] = 255;
                }

                const auto c0 = readU16(block + 8);
                const auto c1 = readU16(block + 10);
                const auto code = readU32(block + 12);

                std::uint8_t colors[4][3]{};
                decode565(c0, colors[0][0], colors[0][1], colors[0][2]);
                decode565(c1, colors[1][0], colors[1][1], colors[1][2]);
                colors[2][0] = static_cast<std::uint8_t>((2 * colors[0][0] + colors[1][0]) / 3);
                colors[2][1] = static_cast<std::uint8_t>((2 * colors[0][1] + colors[1][1]) / 3);
                colors[2][2] = static_cast<std::uint8_t>((2 * colors[0][2] + colors[1][2]) / 3);
                colors[3][0] = static_cast<std::uint8_t>((colors[0][0] + 2 * colors[1][0]) / 3);
                colors[3][1] = static_cast<std::uint8_t>((colors[0][1] + 2 * colors[1][1]) / 3);
                colors[3][2] = static_cast<std::uint8_t>((colors[0][2] + 2 * colors[1][2]) / 3);

                for (std::uint32_t py = 0; py < 4; ++py) {
                    for (std::uint32_t px = 0; px < 4; ++px) {
                        const std::uint32_t x = bx * 4 + px;
                        const std::uint32_t y = by * 4 + py;
                        if (x < width && y < height) {
                            const auto pixelIdx = py * 4 + px;
                            const auto cIdx = (code >> (2 * pixelIdx)) & 0x3;
                            const auto aIdx = static_cast<std::size_t>((aIndices >> (3 * pixelIdx)) & 0x7);
                            const std::size_t outIdx = (y * width + x) * 4;
                            outRgba[outIdx + 0] = colors[cIdx][0];
                            outRgba[outIdx + 1] = colors[cIdx][1];
                            outRgba[outIdx + 2] = colors[cIdx][2];
                            outRgba[outIdx + 3] = alphas[aIdx];
                        }
                    }
                }
            }
        }
        return true;
    } else if (image.format == Format::BC5_RG_UNORM) {
        // Red and Green reconstructed; Blue = sqrt(1 - R^2 - G^2), Alpha = 255
        const std::uint32_t blocksW = (width + 3) / 4;
        const std::uint32_t blocksH = (height + 3) / 4;

        for (std::uint32_t by = 0; by < blocksH; ++by) {
            for (std::uint32_t bx = 0; bx < blocksW; ++bx) {
                const auto* blockR = src + (by * blocksW + bx) * 16;
                const auto* blockG = blockR + 8;

                const auto r0 = blockR[0], r1 = blockR[1];
                std::uint64_t rIndices = 0;
                for (int k = 0; k < 6; ++k) rIndices |= (static_cast<std::uint64_t>(blockR[2 + k]) << (k * 8));
                std::uint8_t reds[8]{}; reds[0] = r0; reds[1] = r1;
                if (r0 > r1) {
                    for (int k = 1; k <= 6; ++k) reds[k + 1] = static_cast<std::uint8_t>(((8 - k) * r0 + k * r1) / 7);
                } else {
                    for (int k = 1; k <= 4; ++k) reds[k + 1] = static_cast<std::uint8_t>(((6 - k) * r0 + k * r1) / 5);
                    reds[6] = 0; reds[7] = 255;
                }

                const auto g0 = blockG[0], g1 = blockG[1];
                std::uint64_t gIndices = 0;
                for (int k = 0; k < 6; ++k) gIndices |= (static_cast<std::uint64_t>(blockG[2 + k]) << (k * 8));
                std::uint8_t greens[8]{}; greens[0] = g0; greens[1] = g1;
                if (g0 > g1) {
                    for (int k = 1; k <= 6; ++k) greens[k + 1] = static_cast<std::uint8_t>(((8 - k) * g0 + k * g1) / 7);
                } else {
                    for (int k = 1; k <= 4; ++k) greens[k + 1] = static_cast<std::uint8_t>(((6 - k) * g0 + k * g1) / 5);
                    greens[6] = 0; greens[7] = 255;
                }

                for (std::uint32_t py = 0; py < 4; ++py) {
                    for (std::uint32_t px = 0; px < 4; ++px) {
                        const std::uint32_t x = bx * 4 + px;
                        const std::uint32_t y = by * 4 + py;
                        if (x < width && y < height) {
                            const auto pixelIdx = py * 4 + px;
                            const auto rIdx = static_cast<std::size_t>((rIndices >> (3 * pixelIdx)) & 0x7);
                            const auto gIdx = static_cast<std::size_t>((gIndices >> (3 * pixelIdx)) & 0x7);
                            const std::size_t outIdx = (y * width + x) * 4;
                            const float rNorm = reds[rIdx] / 255.0f * 2.0f - 1.0f;
                            const float gNorm = greens[gIdx] / 255.0f * 2.0f - 1.0f;
                            const float bNorm = std::sqrt(std::max(0.0f, 1.0f - rNorm * rNorm - gNorm * gNorm));
                            outRgba[outIdx + 0] = reds[rIdx];
                            outRgba[outIdx + 1] = greens[gIdx];
                            outRgba[outIdx + 2] = static_cast<std::uint8_t>(std::clamp((bNorm * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                            outRgba[outIdx + 3] = 255;
                        }
                    }
                }
            }
        }
        return true;
    } else if (image.format == Format::RGBA8) {
        std::copy_n(src, std::min(image.topMipByteSize, outRgba.size()), outRgba.data());
        return true;
    } else if (image.format == Format::BGRA8) {
        for (std::size_t i = 0; i + 3 < image.topMipByteSize && i + 3 < outRgba.size(); i += 4) {
            outRgba[i + 0] = src[i + 2];
            outRgba[i + 1] = src[i + 1];
            outRgba[i + 2] = src[i + 0];
            outRgba[i + 3] = src[i + 3];
        }
        return true;
    } else if (image.format == Format::RGB8) {
        for (std::size_t i = 0, o = 0; i + 2 < image.topMipByteSize && o + 3 < outRgba.size(); i += 3, o += 4) {
            outRgba[o + 0] = src[i + 0];
            outRgba[o + 1] = src[i + 1];
            outRgba[o + 2] = src[i + 2];
            outRgba[o + 3] = 255;
        }
        return true;
    } else if (image.format == Format::BGR8) {
        for (std::size_t i = 0, o = 0; i + 2 < image.topMipByteSize && o + 3 < outRgba.size(); i += 3, o += 4) {
            outRgba[o + 0] = src[i + 2];
            outRgba[o + 1] = src[i + 1];
            outRgba[o + 2] = src[i + 0];
            outRgba[o + 3] = 255;
        }
        return true;
    }

    return false;
}

} // namespace render::dds
