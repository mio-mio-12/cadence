#include "PreparedTexture.h"
#include "TexturePixels.h"
#include "DdsLoader.h"
#include <array>
#include <limits>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <webp/decode.h>
#endif

namespace render::texture {
namespace {
bool usefulAlpha(const std::vector<std::uint8_t>& pixels){
    std::uint8_t low=255,high=0;std::size_t nonOpaque{};const auto count=pixels.size()/4;
    for(std::size_t i=0;i<count;++i){const auto a=pixels[i*4+3];low=std::min(low,a);high=std::max(high,a);if(a<250)++nonOpaque;}
    return high-low>=16&&nonOpaque>count/1000;
}
bool fits(std::uint64_t width,std::uint64_t height,std::uintmax_t fileBytes){
    // Reserve three RGBA planes conservatively (decode/copy/downsample), plus
    // encoded storage. Codec-internal scratch is outside this buffer estimate.
    return width&&height&&width<=65536&&height<=65536&&fileBytes<=preparationBufferBudget&&
        width*height<=(preparationBufferBudget-fileBytes)/12;
}
unsigned internal(const Request& r,bool alpha){return r.compress?(alpha?(r.srgb?0x8C4F:0x83F3):(r.srgb?0x8C4C:0x83F0)):(r.srgb?0x8C43:0x8058);}
void forceAlpha(std::vector<std::uint8_t>& bytes){for(std::size_t i=3;i<bytes.size();i+=4)bytes[i]=255;}
}

Prepared prepare(const Request& r) noexcept {try{
#ifdef _WIN32
    if(r.path.empty()||r.maxDimension<=0||r.maxGpuDimension<=0)return {};
    std::error_code error;const auto fileBytes=std::filesystem::file_size(r.path,error);
    if(error||fileBytes>32ull*1024*1024)return {}; // do not queue large encoded files
    Prepared out;const auto ext=r.path.extension();
    if(ext==L".webp"){
        std::ifstream input(r.path,std::ios::binary);std::vector<std::uint8_t> encoded(static_cast<std::size_t>(fileBytes));
        if(!input.read(reinterpret_cast<char*>(encoded.data()),static_cast<std::streamsize>(encoded.size())))return {};
        int width{},height{};
        if(!WebPGetInfo(encoded.data(),encoded.size(),&width,&height)||!fits(width,height,fileBytes)||width>r.maxGpuDimension||height>r.maxGpuDimension)return {};
        auto* pixels=WebPDecodeRGBA(encoded.data(),encoded.size(),&width,&height);if(!pixels)return {};
        // Release the decoder allocation even if vector allocation throws.
        struct Free {std::uint8_t* p;~Free(){WebPFree(p);}} free{pixels};
        out.bytes.assign(pixels,pixels+static_cast<std::size_t>(width)*height*4);
        downscaleRgba(out.bytes,width,height,r.maxDimension);
        out.usefulAlpha=!r.forceOpaque&&usefulAlpha(out.bytes);if(r.forceOpaque)forceAlpha(out.bytes);
        out.width=width;out.height=height;out.internalFormat=internal(r,out.usefulAlpha);out.ready=true;return out;
    }
    if(ext==L".tga"||ext==L".TGA"){
        std::ifstream in(r.path,std::ios::binary);std::array<unsigned char,18> header{};
        if(!in.read(reinterpret_cast<char*>(header.data()),header.size()))return {};
        const unsigned width=header[12]|(header[13]<<8),height=header[14]|(header[15]<<8);
        if(!fits(width,height,fileBytes))return {};
        TgaImage image;
        if(loadTga(r.path,image,r.maxDimension)&&image.width<=r.maxGpuDimension&&image.height<=r.maxGpuDimension){
            out.width=image.width;out.height=image.height;out.bytes=std::move(image.rgba);
            if(r.forceOpaque)forceAlpha(out.bytes);
            // TGA's legacy classification is BEFORE resizing, unlike WIC/WebP.
            out.usefulAlpha=!r.forceOpaque&&image.hasUsefulAlpha;out.internalFormat=internal(r,out.usefulAlpha);out.ready=true;return out;
        }
    }
    if(ext==L".dds"||ext==L".DDS"){
        std::ifstream in(r.path,std::ios::binary);std::array<unsigned char,128> header{};
        if(!in.read(reinterpret_cast<char*>(header.data()),header.size()))return {};
        const auto u32=[&](int i){return std::uint32_t(header[i])|(std::uint32_t(header[i+1])<<8)|(std::uint32_t(header[i+2])<<16)|(std::uint32_t(header[i+3])<<24);};
        if(!fits(u32(16),u32(12),fileBytes))return {};
        dds::Image image;std::string ddsError;
        if(dds::loadFile(r.path,image,ddsError)&&image.width<=static_cast<unsigned>(r.maxGpuDimension)&&image.height<=static_cast<unsigned>(r.maxGpuDimension)){
            out.width=static_cast<int>(image.width);out.height=static_cast<int>(image.height);
            out.usefulAlpha=image.hasUsefulAlpha;out.mipmaps=!image.isCompressed;
            // Preserve DDS's authored format and existing forceOpaque/sRGB
            // behavior; this performance change does not reinterpret DDS data.
            if(image.isCompressed&&r.compressedUpload&&image.glInternalFormat){
                out.compressed=true;out.internalFormat=image.glInternalFormat;
                image.data.resize(image.topMipByteSize);out.bytes=std::move(image.data);
            }else if(dds::decompressTopMipToRgba(image,out.bytes)){
                downscaleRgba(out.bytes,out.width,out.height,r.maxDimension);out.internalFormat=internal(r,image.hasUsefulAlpha);
            }else if(!image.isCompressed&&!image.data.empty()){
                out.bytes=std::move(image.data);downscaleRgba(out.bytes,out.width,out.height,r.maxDimension);
                out.internalFormat=r.compress?internal(r,image.hasUsefulAlpha):(image.glInternalFormat?image.glInternalFormat:0x8058);
                out.format=image.glFormat?image.glFormat:0x1908;out.type=image.glType?image.glType:0x1401;
            }else return {};
            out.ready=true;return out;
        }
    }
    // WIC objects belong to this worker's apartment, never to the UI factory.
    const auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    struct Apartment {HRESULT hr;~Apartment(){if(SUCCEEDED(hr))CoUninitialize();}} apartment{initialized};
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return {};
    using Microsoft::WRL::ComPtr;ComPtr<IWICImagingFactory> factory;
    auto hr=CoCreateInstance(CLSID_WICImagingFactory2,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(FAILED(hr))hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(FAILED(hr))return {};
    ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateDecoderFromFilename(r.path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)))return {};
    if(FAILED(decoder->GetFrame(0,&frame))||FAILED(factory->CreateFormatConverter(&converter)))return {};
    if(FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return {};
    UINT width{},height{};if(FAILED(converter->GetSize(&width,&height))||!fits(width,height,fileBytes)||width>static_cast<UINT>(r.maxGpuDimension)||height>static_cast<UINT>(r.maxGpuDimension))return {};
    UINT targetWidth=width,targetHeight=height;
    while(targetWidth>static_cast<UINT>(r.maxDimension)||targetHeight>static_cast<UINT>(r.maxDimension)){targetWidth=std::max(1u,targetWidth/2);targetHeight=std::max(1u,targetHeight/2);}
    if(!r.forceOpaque&&(targetWidth!=width||targetHeight!=height)){
        ComPtr<IWICBitmapScaler> scaler;
        if(SUCCEEDED(factory->CreateBitmapScaler(&scaler))&&SUCCEEDED(scaler->Initialize(frame.Get(),targetWidth,targetHeight,WICBitmapInterpolationModeLinear))){
            converter=nullptr;
            if(SUCCEEDED(factory->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))){width=targetWidth;height=targetHeight;}
            else return {};
        }
    }
    const auto stride=static_cast<std::size_t>(width)*4,byteCount=stride*height;
    if(byteCount>std::numeric_limits<UINT>::max())return {};
    out.bytes.resize(byteCount);
    if(FAILED(converter->CopyPixels(nullptr,static_cast<UINT>(stride),static_cast<UINT>(byteCount),out.bytes.data())))return {};
    out.width=static_cast<int>(width);out.height=static_cast<int>(height);
    if(r.forceOpaque){forceAlpha(out.bytes);downscaleRgba(out.bytes,out.width,out.height,r.maxDimension);}
    out.usefulAlpha=!r.forceOpaque&&usefulAlpha(out.bytes);out.internalFormat=internal(r,out.usefulAlpha);
    out.unpackOne=false;out.ready=true;return out;
#else
    (void)r;return {};
#endif
}catch(...){return {};}}
}
