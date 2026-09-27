#include "assets/AssetCatalog.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include "FrozenCompatibilityV250.inc"

int main(int argc,char** argv)try{
    if(argc!=3){std::cerr<<"export-root output-directory\n";return 2;}
    const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
    std::ofstream report(out/"summary.txt"),timings(out/"timings.csv"),inventory(out/"catalog.txt");
    timings<<"kind,iteration,implementation,count,milliseconds,checksum\n";
    report<<"First filesystem scan has uncontrolled OS-cache state; subsequent scans warm. Native catalogue metadata only, no rendering.\n";
    assets::Catalog catalog;std::string error;std::vector<std::string> firstRows;
    for(int iteration=0;iteration<3;++iteration){
        assets::Catalog current;const auto start=std::chrono::steady_clock::now();
        for(const char* game:{"aw","ghosts","bocw_sp","bo2","cso2"}){
            if(!std::filesystem::is_directory(root/game))throw std::runtime_error(std::string("Missing fixture game: ")+game);
            if(!assets::appendScan(root/game,game,current,error))throw std::runtime_error(error);
        }
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        timings<<"scan,"<<iteration<<",production,"<<current.entries.size()<<','<<ms<<",0\n";timings.flush();
        std::vector<std::string> rows;
        for(const auto& asset:current.entries){std::ostringstream row;row<<std::quoted(asset.path.generic_string())<<' '<<std::quoted(asset.name)<<' '<<std::quoted(asset.game)<<' '<<int(asset.role)<<' '<<std::quoted(asset.category);for(const auto& key:asset.compatibilityKeys)row<<' '<<std::quoted(key);rows.push_back(row.str());}
        if(iteration==0){firstRows=rows;catalog=std::move(current);for(const auto& row:rows)inventory<<row<<'\n';}
        else if(rows!=firstRows)throw std::runtime_error("Catalog changed between scans");
        std::cout<<"scan "<<iteration<<" assets="<<rows.size()<<" ms="<<ms<<std::endl;
    }
    std::size_t checked{};
    for(const auto& asset:catalog.entries){const auto expected=frozen_catalog::compatibilityKeys(asset.name,asset.role);if(expected!=asset.compatibilityKeys||expected!=assets::compatibilityKeys(asset.name,asset.role))throw std::runtime_error("Actual name mismatch: "+asset.name);++checked;}
    const std::vector<std::string> adversarial={"","---","SCAR_scar_SCARH","saiga12_saiga_saiga12","MK14_M14ebr_M14SD","ARX_160_arx160","FN57_fnfiveseven_fiveseven","t5_t6_iw3_models_viewmodel_lod0","m4_silencer_grip_scope_mount","r5_RM22_remington_r5rgp","ak47sd_AK_47_ak","m27iar_m27_iar","miniuzi_mini_uzi","TACTICAL_unknown_123","\xC3\xA9_\xFF_alpha",std::string(4096,'a')};
    for(int role=0;role<int(assets::Role::Count);++role)for(const auto& a:adversarial)for(const auto& b:adversarial){const auto name=a+"__"+b;const auto r=static_cast<assets::Role>(role);if(frozen_catalog::compatibilityKeys(name,r)!=assets::compatibilityKeys(name,r))throw std::runtime_error("Generated name mismatch");++checked;}
    for(int iteration=0;iteration<4;++iteration)for(int order=0;order<2;++order){
        const bool frozen=(iteration%2==0?order:1-order)==0;std::uint64_t hash=1469598103934665603ull;
        const auto start=std::chrono::steady_clock::now();
        for(int repetition=0;repetition<3;++repetition)for(const auto& asset:catalog.entries){const auto keys=frozen?frozen_catalog::compatibilityKeys(asset.name,asset.role):assets::compatibilityKeys(asset.name,asset.role);for(const auto& key:keys){for(unsigned char c:key){hash^=c;hash*=1099511628211ull;}hash^=255;hash*=1099511628211ull;}}
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        timings<<"keys,"<<iteration<<','<<(frozen?"frozen":"production")<<','<<catalog.entries.size()*3<<','<<ms<<','<<hash<<'\n';timings.flush();
    }
    report<<"PASS ordered compatibility equality: "<<catalog.entries.size()<<" actual assets; total checks="<<checked<<"; three scan inventories identical\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
