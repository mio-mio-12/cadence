#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace render::environment {
// Disposable, lossless conversion results. Content keys include BOTH inputs;
// exporter updates invalidate the cache even if timestamps are unchanged.
class NativeSkyCache {
    std::filesystem::path root_;
    static constexpr std::uint64_t seed=14695981039346656037ull;
    static void hash(std::uint64_t& h,const void* data,std::size_t size){
        const auto* p=static_cast<const unsigned char*>(data);
        for(std::size_t i=0;i<size;++i){h^=p[i];h*=1099511628211ull;}
    }
public:
    explicit NativeSkyCache(std::filesystem::path root={}):root_(std::move(root)){
        if(root_.empty()){std::error_code ec;root_=std::filesystem::temp_directory_path(ec);if(ec)root_.clear();else root_/="Cadence-NativeSky-v1";}
    }
    static std::optional<std::uint64_t> key(const std::filesystem::path& texture,const std::filesystem::path& mapping){
        std::uint64_t h=seed;
        for(const auto& path:{texture,mapping}){
            std::error_code ec;const auto size=std::filesystem::file_size(path,ec);if(ec||size>256ull*1024*1024)return {};
            hash(h,&size,sizeof(size));std::ifstream in(path,std::ios::binary);if(!in)return {};
            std::array<char,65536> bytes{};std::uint64_t count=0;
            while(in){in.read(bytes.data(),bytes.size());const auto n=in.gcount();hash(h,bytes.data(),static_cast<std::size_t>(n));count+=n;}
            if(!in.eof()||count!=size)return {};
        }return h;
    }
    bool load(std::uint64_t key,unsigned& width,unsigned& height,std::vector<std::uint8_t>& pixels)const{
        if(root_.empty())return false;
        const auto path=root_/(std::to_string(key)+".rgba");std::ifstream in(path,std::ios::binary);if(!in)return false;
        std::array<std::uint64_t,5> header{};in.read(reinterpret_cast<char*>(header.data()),sizeof(header));
        if(!in||header[0]!=0x31594b534143ull||header[1]!=key||header[2]<1024||header[2]>4096||header[3]!=header[2]/2)return false;
        const auto size=header[2]*header[3]*4;std::error_code ec;if(std::filesystem::file_size(path,ec)!=size+sizeof(header)||ec)return false;
        std::vector<std::uint8_t> loaded(static_cast<std::size_t>(size));in.read(reinterpret_cast<char*>(loaded.data()),loaded.size());if(!in)return false;
        auto h=seed;hash(h,loaded.data(),loaded.size());if(h!=header[4])return false;
        width=static_cast<unsigned>(header[2]);height=static_cast<unsigned>(header[3]);pixels=std::move(loaded);return true;
    }
    void save(std::uint64_t key,unsigned width,unsigned height,const std::vector<std::uint8_t>& pixels)const{
        if(root_.empty()||width<1024||width>4096||height!=width/2||pixels.size()!=std::size_t(width)*height*4)return;
        std::error_code ec;std::filesystem::create_directories(root_,ec);if(ec)return;
        // Bound disk growth. Cache exhaustion is only a missed optimization.
        std::uintmax_t used=0;for(std::filesystem::directory_iterator it(root_,ec),end;it!=end&&!ec;it.increment(ec))if(it->is_regular_file(ec))used+=it->file_size(ec);
        if(ec||used+pixels.size()>1024ull*1024*1024)return;
        auto h=seed;hash(h,pixels.data(),pixels.size());const std::array<std::uint64_t,5> header{0x31594b534143ull,key,width,height,h};
        const auto path=root_/(std::to_string(key)+".rgba");
        const auto temporary=root_/(std::to_string(key)+"."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmp");
        {std::ofstream out(temporary,std::ios::binary);out.write(reinterpret_cast<const char*>(header.data()),sizeof(header));out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());out.close();if(!out){std::filesystem::remove(temporary,ec);return;}}
        std::filesystem::remove(path,ec);ec.clear();std::filesystem::rename(temporary,path,ec);if(ec)std::filesystem::remove(temporary,ec);
    }
};
}
