#include "app/ReferenceViewhands.h"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("failed line "+std::to_string(__LINE__));}while(false)
int main(){try{
    using namespace cadence::reference_hands;
    const auto hand=[](const char* path,const char* name,const char* game){assets::Asset a;a.path=path;a.name=name;a.game=game;a.role=assets::Role::ViewHands;return a;};
    assets::Catalog enabled,local;int calls=0;
    enabled.entries={hand("alternate.cast","alternate","CS2"),hand("preferred.cast","VIEWHANDS_MODEL_LOD0","CS2")};
    auto discover=[&](const std::string&,bool){++calls;return local;};
    CHECK(resolve(enabled,"CS2",discover,[](const auto&){return true;})==std::filesystem::path("preferred.cast"));CHECK(calls==0);
    enabled.entries.resize(1);CHECK(resolve(enabled,"cs2",discover,[](const auto&){return true;})==std::filesystem::path("alternate.cast"));CHECK(calls==0);
    enabled.entries.clear();local.entries={hand("cold.cast","viewhands_model","cs2")};
    CHECK(resolve(enabled,"cs2",discover,[](const auto&){return true;})==std::filesystem::path("cold.cast"));CHECK(calls==1);CHECK(enabled.entries.empty());
    calls=0;auto stale=[&](const std::string& game,bool fresh){CHECK(game=="cs2");++calls;assets::Catalog c;c.entries={hand(fresh?"fresh.cast":"missing.cast","viewhands_model","cs2")};return c;};
    CHECK(resolve(enabled,"cs2",stale,[](const auto& p){return p=="fresh.cast";})==std::filesystem::path("fresh.cast"));CHECK(calls==2);
    calls=0;CHECK(!resolve(enabled,"cs2",discover,[](const auto&){return false;}));CHECK(calls==2);
    local.entries={hand("wrong.cast","viewhands_model","mw"),hand("future.cast","universal","future_game")};
    CHECK(resolve(enabled,"future_game",discover,[](const auto&){return true;})==std::filesystem::path("future.cast"));
    local.entries.front().role=assets::Role::ViewWeapon;CHECK(!select(local,"mw"));
    std::cout<<"Reference viewhands tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
