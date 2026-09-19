#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace scene::t6_shared_textures {
inline std::string lower(std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
inline std::optional<std::filesystem::path> siblingRoot(const std::filesystem::path& source){
    for(auto folder=source.parent_path();!folder.empty();){
        if(lower(folder.filename().string())=="bo2_sp")return folder.parent_path()/"bo2";
        const auto parent=folder.parent_path();if(parent==folder)break;folder=parent;
    }return {};
}
inline bool identicalFiles(const std::filesystem::path& a,const std::filesystem::path& b){
    std::error_code error;const auto size=std::filesystem::file_size(a,error);if(error)return false;
    const auto other=std::filesystem::file_size(b,error);if(error||size!=other||size>64*1024*1024)return false;
    std::ifstream left(a,std::ios::binary),right(b,std::ios::binary);if(!left||!right)return false;
    std::array<char,65536> x{},y{};
    while(left){left.read(x.data(),x.size());right.read(y.data(),y.size());if(left.gcount()!=right.gcount()||!std::equal(x.begin(),x.begin()+left.gcount(),y.begin()))return false;}
    return left.eof()&&right.eof();
}
// Called only while importing documents. Index each sibling root once; validate
// duplicate basenames lazily once rather than choosing an arbitrary texture.
class Resolver {
    struct Index {
        std::unordered_map<std::string,std::vector<std::filesystem::path>> files;
        std::unordered_map<std::string,std::optional<std::filesystem::path>> resolved;
    };
    std::mutex mutex_;
    std::unordered_map<std::string,Index> roots_;
public:
    std::filesystem::path resolve(const std::filesystem::path& source,const std::filesystem::path& original){
        if(original.empty())return original;
        std::error_code error;if(std::filesystem::exists(original,error)||error)return original;
        const auto root=siblingRoot(source);if(!root||lower(original.extension().string())!=".png")return original;
        std::lock_guard lock(mutex_);
        const auto key=lower(root->lexically_normal().string());auto found=roots_.find(key);
        if(found==roots_.end()){
            Index index;
            for(std::filesystem::recursive_directory_iterator it(*root,std::filesystem::directory_options::skip_permission_denied,error),end;it!=end;it.increment(error)){
                if(error){error.clear();continue;}if(!it->is_regular_file(error)||lower(it->path().extension().string())!=".png")continue;
                index.files[lower(it->path().filename().string())].push_back(it->path());
            }
            found=roots_.emplace(key,std::move(index)).first;
        }
        auto& index=found->second;const auto name=lower(original.filename().string());auto result=index.resolved.find(name);
        if(result==index.resolved.end()){
            std::optional<std::filesystem::path> candidate;
            if(auto files=index.files.find(name);files!=index.files.end()&&!files->second.empty()){
                std::sort(files->second.begin(),files->second.end());candidate=files->second.front();
                for(std::size_t i=1;i<files->second.size();++i)if(!identicalFiles(*candidate,files->second[i])){candidate.reset();break;}
            }
            result=index.resolved.emplace(name,std::move(candidate)).first;
        }
        return result->second.value_or(original);
    }
};
inline std::filesystem::path resolve(const std::filesystem::path& source,const std::filesystem::path& original){static Resolver resolver;return resolver.resolve(source,original);}
}
