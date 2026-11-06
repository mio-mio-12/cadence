#include "render/PreparedTexture.h"
#include "render/TextureResolutionPolicy.h"
#include "render/TexturePixels.h"
#include "render/BoundedPreparationQueue.h"
#include <atomic>
#include <iostream>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
static void writeMaskPng(const std::filesystem::path& path,const std::vector<std::uint8_t>& pixels){
    using Microsoft::WRL::ComPtr;
    const auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    struct Apartment{HRESULT hr;~Apartment(){if(SUCCEEDED(hr))CoUninitialize();}} apartment{hr};
    const auto ok=[](HRESULT value){if(FAILED(value))throw std::runtime_error("PNG fixture encode failed");};
    ComPtr<IWICImagingFactory> factory;ok(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;ok(factory->CreateStream(&stream));ok(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;ok(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));ok(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;ok(encoder->CreateNewFrame(&frame,nullptr));ok(frame->Initialize(nullptr));ok(frame->SetSize(4,4));
    auto format=GUID_WICPixelFormat32bppBGRA;ok(frame->SetPixelFormat(&format));if(format!=GUID_WICPixelFormat32bppBGRA)throw std::runtime_error("PNG fixture changed channel format");
    auto encoded=pixels;for(size_t i=0;i<encoded.size();i+=4)std::swap(encoded[i],encoded[i+2]);
    ok(frame->WritePixels(4,16,static_cast<UINT>(encoded.size()),encoded.data()));ok(frame->Commit());ok(encoder->Commit());
}
#endif

static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(){try{
    using namespace render::texture;using namespace std::chrono_literals;
    std::vector<int> requests(40);for(int i=0;i<40;++i)requests[i]=i;
    const auto caller=std::this_thread::get_id();std::atomic<int> active{},peak{},produced{},consumed{};
    std::mutex idsMutex;std::set<std::thread::id> ids;int waits=0;
    {
        BoundedPreparationQueue<int,int> queue(requests,[&](const int& n){
            check(std::this_thread::get_id()!=caller,"preparation ran on caller");
            {std::lock_guard lock(idsMutex);ids.insert(std::this_thread::get_id());}
            const int a=++active;int p=peak.load();while(p<a&&!peak.compare_exchange_weak(p,a)){}
            std::this_thread::sleep_for(n%2==0?20ms:1ms);--active;
            check(++produced-consumed<=2,"lookahead exceeds two results");return n*3;
        });
        for(int i=0;i<40;++i)queue.consume([&](const int& n){check(std::this_thread::get_id()==caller,"upload consumer thread changed");check(n==i*3,"results reordered");++consumed;},[&]{check(std::this_thread::get_id()==caller,"wait callback thread changed");++waits;});
    }
    check(peak<=2&&ids.size()<=2&&waits>0,"worker bound/event polling failed");
    std::atomic<int> started{};
    {BoundedPreparationQueue<int,int> abandoned(requests,[&](const int& n){++started;std::this_thread::sleep_for(2ms);return n;});abandoned.consume([](int){},[]{});}
    check(started<=3,"abandoning load processed entire queue");
    bool caught=false;try{BoundedPreparationQueue<int,int> failed(requests,[](int)->int{throw std::runtime_error("decode");});failed.consume([](int){},[]{});}catch(...){caught=true;}check(caught,"loader exception not propagated");
    const auto root=std::filesystem::temp_directory_path()/("cadence-texture-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root);
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}} cleanup{root};
    std::vector<std::uint8_t> bytes(18);bytes[2]=2;bytes[12]=4;bytes[14]=4;bytes[16]=32;bytes[17]=0x28;
    for(int i=0;i<16;++i){bytes.push_back(i*13);bytes.push_back(i*7);bytes.push_back(i*3);bytes.push_back(i*17);}
    const auto path=root/"alpha.tga";{std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
#ifdef _WIN32
    for(bool bgr:{false,true})for(auto size: {std::pair{17,9},std::pair{1,15},std::pair{15,1}}){
        const auto [width,height]=size;std::vector<std::uint8_t> data(128+width*height*3),reference(width*height*4,255);
        const auto u32=[&](size_t offset,unsigned value){for(int i=0;i<4;++i)data[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};
        u32(0,0x20534444);u32(4,124);u32(8,0x100f);u32(12,height);u32(16,width);u32(20,width*3);u32(28,1);u32(76,32);u32(80,64);u32(88,24);u32(92,bgr?0xff0000:0xff);u32(96,0xff00);u32(100,bgr?0xff:0xff0000);
        for(int i=0;i<width*height;++i)for(int c=0;c<3;++c){reference[i*4+c]=static_cast<unsigned char>((i*13+c*91)%256);data[128+i*3+(bgr?2-c:c)]=reference[i*4+c];}
        const auto file=root/"stream.dds";{std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());}
        for(int limit:{1,2,4,32}){auto expected=reference;int w=width,h=height;downscaleRgba(expected,w,h,limit);const auto actual=prepare({file,false,false,false,limit,16384});check(actual.ready&&actual.width==w&&actual.height==h&&actual.bytes==expected,"streamed DDS differs from legacy pixels");}
    }
    for (unsigned fourcc : {0x31545844u,0x33545844u,0x35545844u,0x32495441u,0x30315844u}) {
        const bool dx10=fourcc==0x30315844u;
        const std::size_t header=dx10?148:128,block=fourcc==0x31545844u?8:16;
        std::vector<std::uint8_t> encoded(header+block,0);
        const auto u32=[&](size_t offset,unsigned value){for(int i=0;i<4;++i)encoded[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};
        u32(0,0x20534444);u32(4,124);u32(8,0x81007);u32(12,4);u32(16,4);u32(20,static_cast<unsigned>(block));u32(28,1);
        u32(76,32);u32(80,4);u32(84,fourcc);u32(108,0x1000);
        if(dx10){u32(128,99);u32(132,3);u32(140,1);}
        const auto ddsPath=root/(std::to_string(fourcc)+".dds");
        {std::ofstream out(ddsPath,std::ios::binary);out.write(reinterpret_cast<const char*>(encoded.data()),encoded.size());}
        Request req{ddsPath,false,false,false,4,16384,true};
        const auto linear=prepare(req);req.srgb=true;const auto color=prepare(req);
        const unsigned expectedLinear=dx10?0x8E8C:fourcc==0x31545844u?0x83F1:fourcc==0x33545844u?0x83F2:fourcc==0x35545844u?0x83F3:0x8DBD;
        const unsigned expectedColor=dx10?0x8E8D:fourcc==0x31545844u?0x8C4D:fourcc==0x33545844u?0x8C4E:fourcc==0x35545844u?0x8C4F:0x8DBD;
        check(linear.ready&&color.ready&&linear.compressed&&color.compressed,"DDS native preparation failed");
        check(linear.internalFormat==expectedLinear&&color.internalFormat==expectedColor,"DDS material color space lost");
        check(linear.bytes==color.bytes&&linear.bytes==std::vector<std::uint8_t>(encoded.begin()+header,encoded.end()),"DDS payload changed");
        req.compressedUpload=false;const auto decoded=prepare(req);
        if(!dx10&&fourcc!=0x33545844u)check(decoded.ready&&decoded.internalFormat==0x8C43,"DDS software fallback color space lost");
        else check(!decoded.ready,"unsupported DDS software format must request synchronous fallback");
    }
#endif
#ifdef _WIN32
    {
        std::vector<std::uint8_t> data(64);
        for(int y=0;y<4;++y)for(int x=0;x<4;++x){const auto i=(y*4+x)*4;data[i]=192;data[i+1]=128;data[i+2]=64;data[i+3]=x<2?0:255;}
        const auto png=root/"material-mask.png";writeMaskPng(png,data);
        int w=4,h=4;auto expected=data;downscaleRgba(expected,w,h,2);
        Request req{png,false,false,false,2,16384,true,true};const auto material=prepare(req);
        check(material.ready&&material.width==2&&material.height==2,"mask PNG decode failed");
        check(material.bytes==expected&&material.usefulAlpha,"material resize must preserve RGB beneath zero alpha and average the mask independently");
        req.forceOpaque=true;auto opaque=prepare(req);for(size_t i=3;i<expected.size();i+=4)expected[i]=255;
        check(opaque.ready&&opaque.bytes==expected&&!opaque.usefulAlpha,"opaque material resize preserves artwork");
        req.forceOpaque=false;req.independentChannels=false;const auto coverage=prepare(req);
        check(coverage.ready,"existing transparency resize remains available");
        std::cout<<"zero-alpha RGB: material="<<int(material.bytes[0])<<" coverage="<<int(coverage.bytes[0])<<'\n';
    }
    for(bool opaque:{false,true})for(bool srgb:{false,true})for(bool compress:{false,true}){
        Request req{path,opaque,compress,srgb,2,16384,true};auto prepared=prepare(req);check(prepared.ready,"TGA preparation failed");
        TgaImage legacy;check(loadTga(path,legacy,2),"legacy fixture load failed");if(opaque)for(std::size_t i=3;i<legacy.rgba.size();i+=4)legacy.rgba[i]=255;
        check(prepared.bytes==legacy.rgba&&prepared.width==legacy.width&&prepared.height==legacy.height,"TGA pixel parity failed");
        check(prepared.usefulAlpha==(!opaque&&legacy.hasUsefulAlpha),"alpha policy changed");
    }
    check(mapResolutionSetting(0)==0&&mapResolutionSetting(1024)==1024&&mapResolutionSetting(-1)==256,"Original setting/default validation");
    check(decodeResolutionLimit(0)>65536&&decodeResolutionLimit(2048)==2048,"Original decode limit is still capped");
    const auto original=prepare({path,false,false,true,decodeResolutionLimit(0),16384,true});
    check(original.ready&&original.width==4&&original.height==4&&original.internalFormat==0x8C43,"Original TGA dimensions or color policy lost");
    check(!prepare({path,false,false,false,decodeResolutionLimit(0),2,true}).ready,"Original silently resized to GPU limit");
    bytes[12]=255;bytes[13]=255;bytes[14]=255;bytes[15]=255;{std::ofstream out(root/"huge.tga",std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
    check(!prepare({root/"huge.tga"}).ready,"oversized decode did not request fallback");
#endif
    check(!prepare({root/"missing.png"}).ready,"missing file did not request original fallback");
    {std::ofstream out(root/"broken.png");out<<"bad image";}check(!prepare({root/"broken.png"}).ready,"corrupt file did not request fallback");
    std::cout<<"Texture preparation PASS: two workers/results, ordered caller-thread consumption, waits, teardown/errors, TGA parity, oversized/missing/corrupt fallback\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
