#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace render::dds {

enum class Format {
    Unknown,
    BC1_RGBA,       // DXT1
    BC2_RGBA,       // DXT3
    BC3_RGBA,       // DXT5
    BC4_R_UNORM,    // ATI1 / BC4
    BC4_R_SNORM,
    BC5_RG_UNORM,   // ATI2 / DXN / BC5
    BC5_RG_SNORM,
    BC7_RGBA,       // BPTC BC7
    RGBA8,
    BGRA8,
    RGB8,
    BGR8,
    R8,
    RG8
};

struct Image {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t depth{1};
    std::uint32_t mipCount{1};
    std::uint32_t arraySize{1};
    bool isCubemap{false};
    Format format{Format::Unknown};
    unsigned int glInternalFormat{0};
    unsigned int glFormat{0};
    unsigned int glType{0};
    bool isCompressed{false};
    std::size_t blockSize{0};     // 8 or 16 for BC formats
    std::vector<std::uint8_t> data;
    std::size_t topMipByteSize{0};
    bool hasUsefulAlpha{false};
};

// Parses raw DDS bytes from memory.
bool parse(const std::uint8_t* bytes, std::size_t byteCount, Image& image, std::string& error);

// Loads a .dds file from disk.
bool loadFile(const std::filesystem::path& path, Image& image, std::string& error);

// Fast inspects if the DDS texture contains a meaningful non-uniform alpha channel.
bool checkUsefulAlpha(const Image& image);

// Decompresses top mip to 32-bit RGBA pixels in software (useful for CPU inspection, thumbnail generation, or fallback).
bool decompressTopMipToRgba(const Image& image, std::vector<std::uint8_t>& outRgba);

} // namespace render::dds
