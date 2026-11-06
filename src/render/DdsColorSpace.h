#pragma once
#include "render/DdsLoader.h"

namespace render {
// Material roles determine color space; encoded pixels and alpha stay untouched.
inline unsigned ddsUploadInternalFormat(const dds::Image& image, bool srgb) {
    if (!srgb) return image.glInternalFormat;
    switch (image.format) {
    case dds::Format::BC1_RGBA: return 0x8C4D;
    case dds::Format::BC2_RGBA: return 0x8C4E;
    case dds::Format::BC3_RGBA: return 0x8C4F;
    case dds::Format::BC7_RGBA: return 0x8E8D;
    case dds::Format::RGBA8: case dds::Format::BGRA8: return 0x8C43;
    case dds::Format::RGB8: case dds::Format::BGR8: return 0x8C41;
    default: return image.glInternalFormat; // No sRGB R/RG or BC4/BC5 equivalent.
    }
}
}
