#include "render/EnvironmentSource.h"
#include "render/NativeSkyDome.h"
#include "render/NativeSkyCache.h"
#include "app/SkyLibrary.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <iostream>

int main(){
    using namespace render::environment;
    // Guard the actual embedded shader, not a disconnected CPU implementation:
    // panorama directions use +Y; only the legacy six-face layout flips Y.
    {
        std::ifstream shader(std::filesystem::path(__FILE__).parent_path().parent_path()/"src/render/StageRenderer.cpp");
        assert(shader);const std::string text((std::istreambuf_iterator<char>(shader)),{});
        const auto begin=text.find("vec3 sampleSpecCube(vec3 worldDirection)");assert(begin!=std::string::npos);
        const auto end=text.find("return rgb;}",begin);assert(end!=std::string::npos);
        const auto sample=text.substr(begin,end-begin);
        assert(sample.find("vec3 d=rd;")!=std::string::npos);
        assert(sample.find("atan(d.y,d.x)")!=std::string::npos);
        assert(sample.find("else rgb=specLogicalSample(vec3(d.x,-d.y,d.z));")!=std::string::npos);
        assert(sample.find("vec3 d=vec3(rd.x,-rd.y,rd.z)")==std::string::npos);
        // Imported opaque albedo is not a gloss/F0 map. Guard both direct
        // and cubemap paths in the actual shader; rendered QA is separate.
        assert(text.find("bool importedDielectricFallback=uDielectricByDefault&&!uLens&&!uGltfPbr&&!uSource2Material;")!=std::string::npos);
        assert(text.find("uHasSpecularMap?readSpecularMap(vUv):((uSpecularGlossiness||importedDielectricFallback)?vec4(.04,.04,.04,.5):vec4(base.a,base.a,base.a,base.a))")!=std::string::npos);
        assert(text.find("if(importedDielectricFallback&&!uHasSpecularMap&&uHasRoughnessMap)specularSample.a=1.0-source2Rough;")!=std::string::npos);
        const std::string glossSource="uHasSpecularMap||uHasRoughnessMap||uSpecularGlossiness||importedDielectricFallback";
        size_t matches=0,pos=0;
        while((pos=text.find(glossSource,pos))!=std::string::npos){++matches;pos+=glossSource.size();}
        assert(matches==4); // two legacy lobes, direct PBR and environment PBR
    }
    const auto root=std::filesystem::temp_directory_path()/("cadence-sky-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root/"images");
    Source source;std::string error;
    assert(resolve(root/"regular.hdr",source,error)&&source.layout==Layout::Panorama);
    assert(resolve(root/"T6.IWI",source,error)&&source.layout==Layout::T6Cube);
    const auto write=[&](const auto& path,const auto& text){std::ofstream file(path);file<<text;};
    write(root/"skybox.json",R"({"schema":"firstassault.skybox/1","textures":[{"exportedPath":"images/native.dds","layout":"native_skydome_uv"},{"exportedPath":"images/overlay.dds","layout":"native_effect_mesh_uv"}]})");
    assert(resolve(root/"images/native.dds",source,error)&&source.layout==Layout::FirstAssaultDome);
    assert(resolve(root/"skybox.json",source,error)&&source.texture==root/"images/native.dds");
    assert(resolve(root/"images/overlay.dds",source,error)&&source.layout==Layout::UnsupportedNative);
    assert(!resolve(root/"images/missing.dds",source,error));
    write(root/"images/native.dds","");write(root/"images/overlay.dds","");write(root/"regular.hdr","");write(root/"T6.IWI","");
    cadence::UiFileLists files;cadence::SkyLibrary library;
    const auto skies=library.get(files,root);assert(skies->size()==3);
    assert(std::find(skies->begin(),skies->end(),root/"images/overlay.dds")==skies->end());
    assert(library.get(files,root)==skies);
    write(root/"skybox.json",R"({"schema":"firstassault.skybox/1","textures":[{"exportedPath":"../../outside.dds","layout":"native_skydome_uv"}]})");
    assert(!resolve(root/"skybox.json",source,error));
    write(root/"skybox.json",R"({"schema":"firstassault.skybox/1","schema":"firstassault.skybox/1"})");assert(!resolve(root/"skybox.json",source,error));
    // Octahedral dome fixture deliberately uses a nonlinear, non-latlong UV
    // layout. Its rim is below zero like the native mesh, so horizon rays hit.
    write(root/"mesh.json",R"({"positions":[[0,0,1],[1,0,-0.1],[0,1,-0.1],[-1,0,-0.1],[0,-1,-0.1]],"uv":[[0.5,0.5],[1,0.5],[0.5,0],[0,0.5],[0.5,1]],"indices":[0,1,2,0,2,3,0,3,4,0,4,1]})");
    NativeSkyDome dome;assert(dome.load(root/"mesh.json",error));
    std::array<double,2> uv{},other{};
    assert(dome.sample({0,0,1},uv)&&std::abs(uv[0]-.5)<1e-10&&std::abs(uv[1]-.5)<1e-10);
    assert(dome.sample({1,0,0},uv)&&uv[0]>.9&&std::abs(uv[1]-.5)<1e-10);
    assert(!dome.sample({0,0,-1},uv));
    assert(!dome.sample({0,0,0},uv));assert(!dome.sample({NAN,0,1},uv));
    assert(dome.sample({-1,1e-7,0},uv)&&dome.sample({-1,-1e-7,0},other));assert(std::abs(uv[0]-other[0])<1e-6&&std::abs(uv[1]-other[1])<1e-6);
    std::vector<std::uint8_t> rgba(16*16*4,255),output;
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){const auto i=(y*16+x)*4;rgba[i]=static_cast<std::uint8_t>(x*16);rgba[i+1]=static_cast<std::uint8_t>(y*16);rgba[i+2]=40;}
    assert(dome.panorama(rgba,16,16,64,32,output,error)&&output.size()==64*32*4);
    std::vector<std::uint8_t> serial;assert(dome.panorama(rgba,16,16,64,32,serial,error,false)&&serial==output);
    {NativeSkyCache cache(root/"cache");const auto key=NativeSkyCache::key(root/"mesh.json",root/"mesh.json");assert(key);
     unsigned w=0,h=0;std::vector<std::uint8_t> loaded;
     assert(!cache.load(*key,w,h,loaded));const std::vector<std::uint8_t> pixels(1024*512*4,123);
     cache.save(*key,1024,512,pixels);assert(cache.load(*key,w,h,loaded)&&w==1024&&h==512&&loaded==pixels);
     {std::fstream file(root/"cache"/(std::to_string(*key)+".rgba"),std::ios::in|std::ios::out|std::ios::binary);file.seekp(100);file.put(42);}
     assert(!cache.load(*key,w,h,loaded));cache.save(*key,1024,512,pixels);assert(cache.load(*key,w,h,loaded));
     write(root/"input","AAAA");const auto stamp=std::filesystem::last_write_time(root/"input");const auto first=NativeSkyCache::key(root/"input",root/"mesh.json");
     write(root/"input","BBBB");std::filesystem::last_write_time(root/"input",stamp);assert(first!=NativeSkyCache::key(root/"input",root/"mesh.json"));
     assert(!NativeSkyCache::key(root/"absent",root/"mesh.json"));}
    // Undefined lower hemisphere converges to one rim-average color at nadir,
    // rather than extending azimuth-dependent streaks into a singularity.
    for(int c=0;c<3;++c){int low=255,high=0;for(unsigned x=0;x<64;++x){low=std::min(low,int(output[x*4+c]));high=std::max(high,int(output[x*4+c]));}assert(high-low<=2);}
    // Every authored upper hit still uses the exact barycentric UV and bilinear
    // source pixels, including the previous rounding. No upper sky color blend.
    constexpr double pi=3.14159265358979323846;
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x){const double latitude=((y+.5)/32-.5)*pi,longitude=((x+.5)/64-.5)*2*pi;if(!dome.sample({std::cos(latitude)*std::cos(longitude),std::cos(latitude)*std::sin(longitude),std::sin(latitude)},uv))continue;const auto i=(y*64+x)*4;for(int c=0;c<2;++c){const int expected=static_cast<int>(std::lround(std::clamp(uv[c]*16-.5,0.,15.)*16));assert(output[i+c]==expected);}assert(output[i+2]==40);}
    assert(!dome.panorama(rgba,16,16,9000,32,output,error));
    write(root/"bad.json",R"({"positions":[[0,0,1]],"uv":[[0,0]],"indices":[0,1,2]})");assert(!dome.load(root/"bad.json",error));assert(!dome.sample({0,0,1},uv));
    std::filesystem::remove_all(root);
    std::cout<<"Environment source and native dome tests passed\n";
}
