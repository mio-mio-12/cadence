#include "render/DdsLoader.h"
#include <cassert>
#include <iostream>
#include <vector>

void writeU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t val) {
    if (offset + 2 > buf.size()) buf.resize(offset + 2);
    buf[offset] = static_cast<std::uint8_t>(val & 0xFF);
    buf[offset + 1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
}

void writeU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t val) {
    if (offset + 4 > buf.size()) buf.resize(offset + 4);
    buf[offset] = static_cast<std::uint8_t>(val & 0xFF);
    buf[offset + 1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
    buf[offset + 2] = static_cast<std::uint8_t>((val >> 16) & 0xFF);
    buf[offset + 3] = static_cast<std::uint8_t>((val >> 24) & 0xFF);
}

std::vector<std::uint8_t> createDdsHeader(std::uint32_t width, std::uint32_t height, std::uint32_t fourCC, std::uint32_t pfFlags = 0x4, std::uint32_t rgbBits = 0) {
    std::vector<std::uint8_t> header(128, 0);
    writeU32(header, 0, 0x20534444);  // "DDS "
    writeU32(header, 4, 124);         // dwSize
    writeU32(header, 8, 0x1 | 0x2 | 0x4 | 0x1000); // flags: CAPS, HEIGHT, WIDTH, PIXELFORMAT
    writeU32(header, 12, height);
    writeU32(header, 16, width);
    writeU32(header, 24, 1);          // depth
    writeU32(header, 28, 1);          // mipCount
    writeU32(header, 76, 32);         // pfSize
    writeU32(header, 80, pfFlags);    // pfFlags
    writeU32(header, 84, fourCC);     // fourCC
    writeU32(header, 88, rgbBits);    // rgbBitCount
    return header;
}

void testDdsParseBc1() {
    auto data = createDdsHeader(4, 4, 0x31545844); // "DXT1"
    // 8 bytes BC1 payload: c0 (white 0xFFFF), c1 (black 0x0000), 4 bytes indices (0x00)
    data.insert(data.end(), {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});

    render::dds::Image img;
    std::string err;
    assert(render::dds::parse(data.data(), data.size(), img, err));
    assert(img.width == 4 && img.height == 4);
    assert(img.format == render::dds::Format::BC1_RGBA);
    assert(img.isCompressed);
    assert(img.blockSize == 8);
    assert(img.glInternalFormat == 0x83F1); // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT

    std::vector<std::uint8_t> rgba;
    assert(render::dds::decompressTopMipToRgba(img, rgba));
    assert(rgba.size() == 4 * 4 * 4);
    // Index 0 should be pure white
    assert(rgba[0] == 255 && rgba[1] == 255 && rgba[2] == 255 && rgba[3] == 255);
    std::cout << "[PASS] testDdsParseBc1\n";
}

void testDdsParseBc3() {
    auto data = createDdsHeader(4, 4, 0x35545844); // "DXT5"
    // 16 bytes BC3 payload: 8 bytes alpha (a0=255, a1=0, indices=0), 8 bytes BC1 color
    data.insert(data.end(), {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                             0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});

    render::dds::Image img;
    std::string err;
    assert(render::dds::parse(data.data(), data.size(), img, err));
    assert(img.format == render::dds::Format::BC3_RGBA);
    assert(img.blockSize == 16);
    assert(img.glInternalFormat == 0x83F3); // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT

    std::vector<std::uint8_t> rgba;
    assert(render::dds::decompressTopMipToRgba(img, rgba));
    assert(rgba.size() == 4 * 4 * 4);
    assert(rgba[3] == 255); // Top-left pixel alpha should match a0 (255)
    std::cout << "[PASS] testDdsParseBc3\n";
}

void testDdsParseBc5NormalMap() {
    auto data = createDdsHeader(4, 4, 0x32495441); // "ATI2" / BC5
    // 16 bytes BC5 payload: 8 bytes Red (128 = normal X 0), 8 bytes Green (128 = normal Y 0)
    data.insert(data.end(), {0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                             0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});

    render::dds::Image img;
    std::string err;
    assert(render::dds::parse(data.data(), data.size(), img, err));
    assert(img.format == render::dds::Format::BC5_RG_UNORM);
    assert(img.blockSize == 16);
    assert(img.glInternalFormat == 0x8DBD); // GL_COMPRESSED_RG_RGTC2

    std::vector<std::uint8_t> rgba;
    assert(render::dds::decompressTopMipToRgba(img, rgba));
    assert(rgba.size() == 4 * 4 * 4);
    // Blue normal should be reconstructed towards positive Z (near 255)
    assert(rgba[0] == 0x80 && rgba[1] == 0x80);
    assert(rgba[2] >= 250);
    std::cout << "[PASS] testDdsParseBc5NormalMap\n";
}

void testDdsParseDx10Header() {
    auto data = createDdsHeader(8, 8, 0x30315844); // "DX10"
    // Append 20-byte DX10 header
    data.resize(148, 0);
    writeU32(data, 128, 98); // DXGI_FORMAT_BC7_UNORM
    writeU32(data, 132, 3);  // D3D10_RESOURCE_DIMENSION_TEXTURE2D
    writeU32(data, 140, 1);  // arraySize

    // 8x8 is 4 blocks of BC7 (4 * 16 = 64 bytes)
    data.resize(148 + 64, 0);

    render::dds::Image img;
    std::string err;
    assert(render::dds::parse(data.data(), data.size(), img, err));
    assert(img.width == 8 && img.height == 8);
    assert(img.format == render::dds::Format::BC7_RGBA);
    assert(img.glInternalFormat == 0x8E8C); // GL_COMPRESSED_RGBA_BPTC_UNORM_ARB
    assert(img.topMipByteSize == 64);
    std::cout << "[PASS] testDdsParseDx10Header\n";
}

void testDdsUncompressedRgba() {
    auto data = createDdsHeader(2, 2, 0, 0x41, 32); // DDPF_RGBA 32-bit
    writeU32(data, 92, 0x00FF0000); // R mask
    writeU32(data, 96, 0x0000FF00); // G mask
    writeU32(data, 100, 0x000000FF); // B mask
    writeU32(data, 104, 0xFF000000); // A mask

    // 2x2 32-bit pixels (16 bytes)
    data.insert(data.end(), {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                             0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00});

    render::dds::Image img;
    std::string err;
    assert(render::dds::parse(data.data(), data.size(), img, err));
    assert(img.width == 2 && img.height == 2);
    assert(img.format == render::dds::Format::BGRA8);
    assert(!img.isCompressed);
    assert(img.topMipByteSize == 16);
    std::cout << "[PASS] testDdsUncompressedRgba\n";
}

void testDdsTruncated() {
    std::vector<std::uint8_t> shortData(64, 0);
    render::dds::Image img;
    std::string err;
    assert(!render::dds::parse(shortData.data(), shortData.size(), img, err));
    assert(!err.empty());
    std::cout << "[PASS] testDdsTruncated\n";
}

int main() {
    testDdsParseBc1();
    testDdsParseBc3();
    testDdsParseBc5NormalMap();
    testDdsParseDx10Header();
    testDdsUncompressedRgba();
    testDdsTruncated();
    std::cout << "All DDS Loader tests passed!\n";
    return 0;
}
