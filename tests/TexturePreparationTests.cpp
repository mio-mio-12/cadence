#include "render/PreparedTexture.h"
#include "render/TexturePixels.h"
#include "render/BoundedPreparationQueue.h"
#include <atomic>
#include <iostream>
#include <set>
#include <stdexcept>

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
    for(bool opaque:{false,true})for(bool srgb:{false,true})for(bool compress:{false,true}){
        Request req{path,opaque,compress,srgb,2,16384,true};auto prepared=prepare(req);check(prepared.ready,"TGA preparation failed");
        TgaImage legacy;check(loadTga(path,legacy,2),"legacy fixture load failed");if(opaque)for(std::size_t i=3;i<legacy.rgba.size();i+=4)legacy.rgba[i]=255;
        check(prepared.bytes==legacy.rgba&&prepared.width==legacy.width&&prepared.height==legacy.height,"TGA pixel parity failed");
        check(prepared.usefulAlpha==(!opaque&&legacy.hasUsefulAlpha),"alpha policy changed");
    }
    bytes[12]=255;bytes[13]=255;bytes[14]=255;bytes[15]=255;{std::ofstream out(root/"huge.tga",std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
    check(!prepare({root/"huge.tga"}).ready,"oversized decode did not request fallback");
#endif
    check(!prepare({root/"missing.png"}).ready,"missing file did not request original fallback");
    {std::ofstream out(root/"broken.png");out<<"bad image";}check(!prepare({root/"broken.png"}).ready,"corrupt file did not request fallback");
    std::cout<<"Texture preparation PASS: two workers/results, ordered caller-thread consumption, waits, teardown/errors, TGA parity, oversized/missing/corrupt fallback\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
