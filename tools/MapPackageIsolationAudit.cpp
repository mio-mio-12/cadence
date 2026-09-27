#include "scene/GlbMap.h"
#include "scene/BoundedJson.h"
#include "scene/MapTextureCache.h"
#include "content/AssetPaths.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <chrono>
namespace fs=std::filesystem;
using namespace cadence::content;
void write(const fs::path& p,const std::string& data){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write(data.data(),data.size());if(!f)throw std::runtime_error("fixture write failed");}
std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("fixture read failed");return {std::istreambuf_iterator<char>(f),{}};}
void be(std::string& s,std::uint32_t n){for(int i=3;i>=0;--i)s+=char(n>>(i*8));}
void le(std::string& s,std::uint32_t n){for(int i=0;i<4;++i)s+=char(n>>(i*8));}
void chunk(std::string& png,const char* type,const std::string& bytes){
    be(png,static_cast<std::uint32_t>(bytes.size()));std::string data=std::string(type,4)+bytes;png+=data;
    std::uint32_t crc=~0u;for(unsigned char c:data){crc^=c;for(int i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u&-(crc&1));}be(png,~crc);
}
std::string pixel(bool red){
    std::string png("\x89PNG\r\n\x1a\n",8),ihdr;be(ihdr,1);be(ihdr,1);ihdr.append("\x08\x06\0\0\0",5);chunk(png,"IHDR",ihdr);
    std::string raw;raw+=char(0);raw+=char(red?255:0);raw+=char(0);raw+=char(red?0:255);raw+=char(255);
    std::string z("\x78\x01\x01\x05\0\xfa\xff",7);z+=raw;std::uint32_t a=1,b=0;for(unsigned char c:raw){a=(a+c)%65521;b=(b+a)%65521;}be(z,(b<<16)|a);
    chunk(png,"IDAT",z);chunk(png,"IEND",{});return png;
}
void fixture(const fs::path& path,float offset,bool embedded,bool red){
    const float vertices[]{-1+offset,0,-1,1+offset,0,-1,offset,0,1};
    std::string bin(reinterpret_cast<const char*>(vertices),sizeof(vertices));
    auto j=scene::codm::parseJson(R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],"buffers":[{"byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"images":[{}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}]})");
    const auto png=pixel(red);
    if(!embedded){j["buffers"][0]["uri"]="geometry.dat";j["images"][0]["uri"]="texture.png";write(path.parent_path()/"geometry.dat",bin);write(path.parent_path()/"texture.png",png);write(path,j.dump());return;}
    j["bufferViews"].push_back({{"buffer",0},{"byteOffset",36},{"byteLength",png.size()}});j["images"][0]={{"bufferView",1},{"mimeType","image/png"}};bin+=png;
    while(bin.size()%4)bin+='\0';j["buffers"][0]["byteLength"]=bin.size();std::string json=j.dump();while(json.size()%4)json+=' ';
    std::string glb;le(glb,0x46546c67);le(glb,2);le(glb,static_cast<std::uint32_t>(28+json.size()+bin.size()));le(glb,static_cast<std::uint32_t>(json.size()));le(glb,0x4e4f534a);glb+=json;le(glb,static_cast<std::uint32_t>(bin.size()));le(glb,0x004e4942);glb+=bin;write(path,glb);
}
int main(int argc,char** argv)try{
    if(argc>2){std::cerr<<"optional fresh output directory\n";return 2;}
    const bool temporary=argc==1;
    const fs::path root=fs::absolute(temporary?fs::temp_directory_path()/("cadence_map_package_isolation_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())):fs::u8path(argv[1]));
    if(fs::exists(root))throw std::runtime_error("Use a fresh audit output directory");fs::create_directories(root);
    struct Cleanup{fs::path path;~Cleanup(){if(!path.empty()){std::error_code ec;fs::remove_all(path,ec);}}}cleanup{temporary?root:fs::path{}};
    std::ofstream log(root/"results.txt");int failures=0;const auto check=[&](bool okay,const char* name){log<<(okay?"PASS ":"FAIL ")<<name<<std::endl;if(!okay){std::cerr<<"FAIL "<<name<<'\n';++failures;}};
    const auto load=[&](const fs::path& p,scene::glb::Map& m){std::string error;if(!scene::glb::load(p,m,error,1,false))throw std::runtime_error(error);if(m.scene.meshes.empty())throw std::runtime_error("fixture mesh absent");};
    const auto a=root/"embedded-a"/"world.glb",b=root/"embedded-b"/"world.glb";fixture(a,0,true,true);fixture(b,3,true,false);
    scene::glb::Map ma,mb;load(a,ma);const auto retained=ma.scene.meshes.front().albedoPath;check(read(retained)==pixel(true),"A embedded image initially red");load(b,mb);
    check(retained!=mb.scene.meshes.front().albedoPath,"same-stem owners have distinct extraction paths");check(read(retained)==pixel(true),"A retained embedded image unchanged after B");check(read(mb.scene.meshes.front().albedoPath)==pixel(false),"B embedded image blue");
    scene::glb::Map unchanged;load(a,unchanged);
    check(unchanged.scene.meshes.front().albedoPath==retained,"unchanged source reuses extraction namespace");
    const auto stamp=fs::last_write_time(a);fixture(a,0,true,false);
    fs::last_write_time(a,stamp+std::chrono::seconds(1));
    scene::glb::Map revised;load(a,revised);
    check(revised.scene.meshes.front().albedoPath!=retained,"modified source revision gets distinct namespace");
    check(read(revised.scene.meshes.front().albedoPath)==pixel(false),"modified source image has new bytes");
    check(read(retained)==pixel(true),"modified source preserves retained old image bytes");
    const auto c=root/"occupied-owner"/"world.glb";fixture(c,0,true,true);
    // This namespace was created only for this fresh diagnostic source. Force
    // an identity mismatch without relying on finding a real hash collision.
    const auto occupied=scene::glb::textureCacheDirectory(c);
    write(occupied/"source.txt","different owner diagnostic sentinel");
    scene::glb::Map collision;load(c,collision);
    check(collision.scene.meshes.front().albedoPath.parent_path()!=occupied,"occupied wrong-owner bucket gets another namespace");
    check(read(occupied/"source.txt")=="different owner diagnostic sentinel","occupied owner identity is not overwritten");
    check(read(collision.scene.meshes.front().albedoPath)==pixel(true),"collision fallback retains requested image bytes");
    const auto pa=root/"pack-a"/"world.gltf",pb=root/"pack-b"/"world.gltf";fixture(pa,0,false,true);fixture(pb,3,false,false);
    PathMount mountA{"a",{},pa.parent_path()},mountB{"b",{},pb.parent_path()};
    for(const auto name:{"world.gltf","geometry.dat","texture.png"}){mountA.paths["game:maps/"+std::string(name)]=pa.parent_path()/name;mountB.paths["game:maps/"+std::string(name)]=pb.parent_path()/name;}
    mountPaths({mountA,mountB});preferPack("b");load(pa,ma);load(pb,mb);
    check(std::abs(ma.scene.meshes.front().vertices.front().position.x+100)<.01f,"A buffer remains local with B preferred");check(std::abs(mb.scene.meshes.front().vertices.front().position.x-200)<.01f,"B distinct buffer loaded");
    check(read(ma.scene.meshes.front().albedoPath)==pixel(true),"A external image remains local");
    fs::rename(pa.parent_path()/"geometry.dat",pa.parent_path()/"geometry.saved");std::string error;scene::glb::Map missing;
    check(!scene::glb::load(pa,missing,error,1,false),"missing A buffer cannot borrow B");
    mountPaths({});preferPack({});log<<"failures="<<failures<<std::endl;return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 3;}
