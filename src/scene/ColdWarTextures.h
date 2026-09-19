#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

namespace scene::coldwar_textures {
inline std::filesystem::path accessible(std::filesystem::path path){
#ifdef _WIN32
    if(path.native().size()>=248&&!path.native().starts_with(L"\\\\?\\")){
        std::error_code ec;path=std::filesystem::absolute(path,ec).lexically_normal();if(ec)return path;
        auto s=path.make_preferred().wstring();return s.starts_with(L"\\\\")?std::filesystem::path(L"\\\\?\\UNC\\"+s.substr(2)):std::filesystem::path(L"\\\\?\\"+s);
    }
#endif
    return path;
}
// Exact sibling/material matches only. Never search other weapons or pick an
// arbitrary image by extension. This runs during import, not while rendering.
inline std::filesystem::path fallback(const std::filesystem::path& model,const std::string& material,const std::filesystem::path& original){
    std::error_code ec;const auto readable=accessible(original);if(original.empty()||std::filesystem::is_regular_file(readable,ec))return readable;
    const auto folder=model.parent_path();const auto name=folder.filename().string();
    if(!name.starts_with("wpn_t9_"))return original;
    if(name.find("_scope")==std::string::npos&&name.find("_mag")==std::string::npos&&name.find("_silencer")==std::string::npos&&name.find("_suppressor")==std::string::npos&&name.find("_grip")==std::string::npos&&name.find("_barrel")==std::string::npos&&name.find("_stock")==std::string::npos)return original;
    std::vector<std::filesystem::path> matches;
    for(std::size_t first=name.find('_',7);first!=std::string::npos;first=name.find('_',first+1)){
        for(std::size_t end=name.find('_',first+1);end!=std::string::npos;end=name.find('_',end+1)){
            const auto removed=name.substr(first,end-first);
            if(removed=="_scope"||removed=="_ads"||removed=="_view"||removed=="_world"||removed=="_mag")continue;
            const auto base=folder.parent_path()/(name.substr(0,first)+name.substr(end));
            if(!std::filesystem::is_directory(base,ec))continue;
            auto mat=material;if(mat.starts_with("mc_"))mat.erase(0,3);
            auto file=original.filename().string();
            const auto remove=[&](std::string value){for(auto p=value.find(removed);p!=std::string::npos;p=value.find(removed,p)){const auto after=p+removed.size();if(after==value.size()||value[after]=='_'||value[after]=='.')value.erase(p,removed.size());else ++p;}return value;};
            const auto baseMat=remove(mat),baseFile=remove(file);
            auto imageAlias=baseFile;if(imageAlias.starts_with("i_mtl_"))imageAlias.erase(2,4);
            for(const auto& candidate:{base/"_images"/"mc"/mat/file,base/"_images"/"mc"/baseMat/baseFile,base/"_images"/"mc"/baseMat/imageAlias}){
                if(std::filesystem::is_regular_file(accessible(candidate),ec))matches.push_back(accessible(candidate));
            }
        }
    }
    std::sort(matches.begin(),matches.end());matches.erase(std::unique(matches.begin(),matches.end()),matches.end());
    return matches.size()==1?matches.front():original;
}
}
