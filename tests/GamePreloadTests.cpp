#include "app/GamePreload.h"
#include <fstream>
#include <iostream>
#include <array>

int main(int argc,char** argv){
    if(argc==2){
        const auto measure=[&]{const auto start=std::chrono::steady_clock::now();std::size_t nodes{};for(int i=0;i<5;++i){const auto d=cast::Document::load(argv[1]);if(!d.valid())return -1.;nodes+=d.statistics().nodes;}std::cout<<"nodes="<<nodes<<' ';return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/5.;};
        const auto cold=measure();const auto bytes=cast::Document::preload(argv[1]);const auto cached=measure();cast::Document::releasePreload(argv[1]);std::cout<<"uncached_ms="<<cold<<" cached_ms="<<cached<<" resident_bytes="<<bytes<<'\n';return cold<0||cached<0||!bytes?1:0;
    }
    namespace fs=std::filesystem;int failures=0;
    const auto check=[&](bool ok,const char* name){if(!ok){std::cerr<<"FAIL "<<name<<'\n';++failures;}};
    const auto root=fs::temp_directory_path()/("cadence-preload-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);const auto file=root/"empty.cast";
    const auto write=[&](bool valid){std::array<std::uint32_t,4> bytes{valid?cast::Document::kMagic:0,1,0,0};std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),sizeof(bytes));};
    write(true);
    check(cast::Document::load(file).valid(),"fixture valid");
    check(cast::Document::preload(file)>0,"document retained");
    auto retained=cast::Document::load(file);cast::Document::releasePreload(file);
    check(retained.valid(),"existing reader survives release");
    check(cast::Document::preload(file)>0,"repreload");
    write(false);fs::last_write_time(file,fs::file_time_type::clock::now()+std::chrono::seconds(3));
    check(!cast::Document::load(file).valid(),"modified file invalidates cache");
    cast::Document::releasePreload(file);write(true);
    const auto texture=root/"pixel.tga";
    {std::array<unsigned char,22> tga{};tga[2]=2;tga[12]=1;tga[14]=1;tga[16]=32;tga[17]=0x28;tga[18]=25;tga[19]=80;tga[20]=170;tga[21]=255;std::ofstream out(texture,std::ios::binary);out.write(reinterpret_cast<const char*>(tga.data()),tga.size());}
    render::texture::Request request;request.path=texture;
    const auto before=render::texture::prepare(request);
    check(before.ready,"texture fixture decodes");
    check(render::texture::preload(request)>0,"texture retained");
    const auto after=render::texture::prepare(request);
    check(after.bytes==before.bytes&&after.internalFormat==before.internalFormat,"cached pixels identical");
    render::texture::releasePreload(request);
    {
        cadence::GamePreload manager;
        manager.estimate(root);
        for(int i=0;i<500&&manager.status(root).stage!="Estimate ready";++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
        check(manager.status(root).stage=="Estimate ready"&&manager.bytes()==0,"estimate without residency");
        check(manager.status(root).sourceBytes==16&&manager.status(root).textureBytes==22,"estimate source accounting");
        manager.pause(true);
        const auto begin=std::chrono::steady_clock::now();manager.select(root,true);
        check(std::chrono::steady_clock::now()-begin<std::chrono::milliseconds(50),"selection nonblocking");
        std::this_thread::sleep_for(std::chrono::milliseconds(30));check(manager.bytes()==0,"pause prevents cache work");
        manager.pause(false);
        const auto wait=[&](auto predicate){for(int i=0;i<500;++i){if(predicate())return true;std::this_thread::sleep_for(std::chrono::milliseconds(10));}return false;};
        check(wait([&]{return manager.status(root).stage=="CPU assets cached";}),"background completion");
        check(manager.bytes()>0,"resident byte reporting");
        check(manager.status(root).failed==0,"no false failures");
        manager.select(root,false);check(wait([&]{return manager.bytes()==0&&manager.status(root).stage=="Not loaded";}),"async release");
        manager.budget(1);manager.select(root,true);
        check(wait([&]{return manager.status(root).stage.find("Memory limit")!=std::string::npos;}),"budget enforced");
        manager.pause(true);manager.select(root,false);check(wait([&]{return manager.status(root).stage=="Not loaded";}),"cancel while paused");
    }
    fs::remove_all(root);
    if(!failures)std::cout<<"Game preload tests passed\n";return failures?1:0;
}
