#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace render::texture {
struct Request {
    std::filesystem::path path;
    bool forceOpaque{},compress{},srgb{};
    int maxDimension{1024},maxGpuDimension{16384};
    bool compressedUpload{true};
};
struct Prepared {
    // false means use the original loader, including its error/fallback policy.
    bool ready{},compressed{},mipmaps{true},usefulAlpha{},unpackOne{true};
    int width{},height{};
    unsigned internalFormat{},format{0x1908},type{0x1401}; // RGBA / UNSIGNED_BYTE
    std::vector<std::uint8_t> bytes;
};
// No GL calls or renderer state. Each invocation owns its decoder/COM objects.
// Known file/pixel buffers must fit 128 MiB; larger inputs stay synchronous.
inline constexpr std::size_t preparationBufferBudget=128ull*1024*1024;
Prepared prepare(const Request& request) noexcept;
}
