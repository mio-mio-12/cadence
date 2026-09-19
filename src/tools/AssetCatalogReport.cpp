#include "assets/AssetCatalog.h"

#include <filesystem>
#include <iostream>

int main(int argc,char** argv){if(argc!=2){std::cerr<<"Usage: asset_catalog_report <saluki-root>\n";return 2;}assets::Catalog catalog;std::string error;
    if(!assets::scan(std::filesystem::u8path(argv[1]),catalog,error)){std::cerr<<error<<'\n';return 1;}
    std::cout<<"Cast files: "<<catalog.scannedCastFiles<<"  catalog assets: "<<catalog.entries.size()<<"  skipped higher LODs: "<<catalog.skippedLods<<'\n';
    for(std::size_t i=0;i<static_cast<std::size_t>(assets::Role::Count);++i)std::cout<<assets::roleName(static_cast<assets::Role>(i))<<": "<<catalog.counts[i]<<'\n';
    std::size_t weapons{},withoutAttachments{};for(std::size_t i=0;i<catalog.entries.size();++i){const auto role=catalog.entries[i].role;if(role!=assets::Role::WorldWeapon&&role!=assets::Role::ViewWeapon)continue;
        ++weapons;if(catalog.compatibleAttachments(i).empty())++withoutAttachments;}
    std::cout<<"Weapons: "<<weapons<<"  without inferred compatible attachments: "<<withoutAttachments<<'\n';return 0;}
