#pragma once
#include "assets/AssetCatalog.h"
#include "assets/Source2Skins.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

namespace cadence {
// Owned by one AppState, on its UI thread. Catalog mutation boundaries clear
// this cache, including in-place edits that keep the allocation and size.
class WeaponPickerCache {
    const assets::Asset* data_{};std::size_t size_{};bool indexed_{};
    struct Winner {std::string name;bool base{};};
    std::unordered_map<std::string,Winner> coldWar_;
    bool importedIndexed_{};
    bool source2Indexed_{};
    std::unordered_map<std::string,const assets::Asset*> source2Winners_;
    std::unordered_map<std::string,std::string> importedWinners_;
    std::map<std::pair<std::string,int>,std::vector<std::size_t>> pools_;
    static std::string lower(std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
    static std::string family(const assets::Asset& a){const auto g=lower(a.game);return a.role==assets::Role::ViewWeapon&&(g=="bocw_sp"||g=="bocw"||g=="t9")?assets::coldWarWeaponFamily(a.name):std::string{};}
    void bind(const assets::Catalog& c){if(data_!=c.entries.data()||size_!=c.entries.size()){clear();data_=c.entries.data();size_=c.entries.size();}}
public:
    static bool knifeCategory(const assets::Asset& a){
        const auto category=lower(a.category),name=lower(a.name);
        if(category.find("knife")!=std::string::npos||category.find("melee")!=std::string::npos)return true;
        for(const auto token:{"knife","knives","bayonet","karambit","dagger","kukri","kunai","machete","navaja","falchion","butterfly","talon","katana"})
            if(name.find(token)!=std::string::npos)return true;
        return false;
    }
    std::size_t indexBuilds{},poolBuilds{};
    void clear(){data_=nullptr;size_=0;indexed_=false;importedIndexed_=false;source2Indexed_=false;source2Winners_.clear();coldWar_.clear();importedWinners_.clear();pools_.clear();}
    bool isSource2SkinVariant(const assets::Catalog& c,const assets::Asset& a){
        const auto f=assets::source2::weaponFamily(a);if(f.empty())return false;bind(c);
        if(!source2Indexed_){for(const auto& candidate:c.entries){const auto cf=assets::source2::weaponFamily(candidate);if(cf.empty())continue;
            const auto key=std::to_string(static_cast<int>(candidate.role))+":"+cf;
            auto [it,inserted]=source2Winners_.try_emplace(key,&candidate);
            if(!inserted&&assets::source2::precedes(candidate,*it->second))it->second=&candidate;
        }source2Indexed_=true;}
        return source2Winners_.at(std::to_string(static_cast<int>(a.role))+":"+f)->path!=a.path;
    }
    static std::string importedFamily(const assets::Asset& a){
        if(lower(a.game)!="eldewrito"||a.role!=assets::Role::ViewWeapon)return {};
        const auto name=lower(a.name);const auto fp=name.rfind("_fp_");if(fp==std::string::npos)return {};
        return name.substr(0,fp)+(name.ends_with("_dual_weapon")?"_dual":"");
    }
    bool isImportedVariant(const assets::Catalog& c,const assets::Asset& a){
        const auto f=importedFamily(a);if(f.empty())return false;bind(c);
        if(!importedIndexed_){for(const auto& candidate:c.entries){const auto key=importedFamily(candidate);if(key.empty())continue;auto [it,inserted]=importedWinners_.try_emplace(key,candidate.name);if(!inserted&&candidate.name<it->second)it->second=candidate.name;}importedIndexed_=true;}
        const auto it=importedWinners_.find(f);return it!=importedWinners_.end()&&it->second!=a.name;
    }
    bool isColdWarVariant(const assets::Catalog& c,const assets::Asset& a){
        const auto f=family(a);if(f.empty())return false;bind(c);
        if(!indexed_){
            for(const auto& candidate:c.entries){const auto cf=family(candidate);if(cf.empty())continue;
                const auto key=candidate.game+'\x1f'+cf;const bool base=assets::coldWarWeaponVariant(candidate.name).empty();
                auto [it,inserted]=coldWar_.try_emplace(key,Winner{candidate.name,base});
                if(!inserted&&((base&&!it->second.base)||(base==it->second.base&&candidate.name<it->second.name)))it->second={candidate.name,base};
            }indexed_=true;++indexBuilds;
        }
        const auto it=coldWar_.find(a.game+'\x1f'+f);
        return it!=coldWar_.end()&&it->second.name!=a.name;
    }
    template<class Build> const std::vector<std::size_t>& pool(const assets::Catalog& c,std::string game,int category,Build&& build){
        bind(c);const auto key=std::make_pair(lower(std::move(game)),category);
        if(const auto it=pools_.find(key);it!=pools_.end())return it->second;
        auto result=std::forward<Build>(build)();++poolBuilds;
        return pools_.emplace(key,std::move(result)).first->second;
    }
};
}
