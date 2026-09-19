#pragma once
#include "app/TimedSnapshotCache.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <vector>

namespace cadence {
class UiFileLists {
public:
    using Files=std::vector<std::filesystem::path>;
    using Clock=TimedSnapshotCache<Files>::Clock;
    std::shared_ptr<const Files> get(const std::filesystem::path& root,const std::string& extensions,bool recursive=false,bool force=false,Clock::time_point now=Clock::now()){
        const auto key=root.lexically_normal().string()+"|"+extensions+(recursive?"|r":"|d");
        return caches_[key].get(key,now,force,[&]{
            ++scans_;Files result;std::error_code error;
            const auto add=[&](const auto& entry){
                if(!entry.is_regular_file(error))return;
                auto ext=entry.path().extension().string();for(auto& c:ext)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if(extensions.empty()||extensions.find("|"+ext+"|")!=std::string::npos)result.push_back(entry.path());
            };
            if(recursive){for(std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,error),end;it!=end;it.increment(error)){if(error){error.clear();continue;}add(*it);}}
            else {for(std::filesystem::directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,error),end;it!=end;it.increment(error)){if(error){error.clear();continue;}add(*it);}}
            std::sort(result.begin(),result.end());return result;
        });
    }
    std::size_t scans()const{return scans_;}
private:
    std::unordered_map<std::string,TimedSnapshotCache<Files>> caches_;
    std::size_t scans_{};
};
}
