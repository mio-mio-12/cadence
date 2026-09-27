#pragma once
#include "scene/CastScene.h"
#include "take/AtomicFile.h"
#include <map>
#include <fstream>
#include <iomanip>
#include <tuple>
#include <sstream>

namespace cadence {
struct AnimationSet {
    using Key=std::tuple<std::string,int,std::string>;
    // Action, motion, stance, direction, weapon, ADS (-1 means either).
    using Slot=std::tuple<int,int,int,int,int,int>;
    std::map<Slot,Key> overrides;
    std::map<Key,bool> rules;
    std::map<std::string,std::string> models;
    bool curatedDeaths{};
    bool operator==(const AnimationSet&) const = default;
    static std::string file(const std::string& name){return std::filesystem::path(name).filename().string();}
    bool enabled(const std::string& game,scene::ActionRole action,const std::string& name)const{
        const auto it=rules.find({game,static_cast<int>(action),file(name)});
        return it!=rules.end()?it->second:!(curatedDeaths&&action==scene::ActionRole::Death);
    }
    bool explicitEnabled(const scene::Animation& clip)const{
        const auto it=rules.find({clip.sourceGame,static_cast<int>(clip.action),file(clip.sourceName)});return it!=rules.end()&&it->second;
    }
    void set(const std::string& game,scene::ActionRole action,const std::string& name,bool enabled){rules[{game,static_cast<int>(action),file(name)}]=enabled;}
    static Slot slot(const scene::Animation& a){return {int(a.action),int(a.action==scene::ActionRole::None?a.motion:scene::MotionRole::Unknown),int(a.stance),int(a.action==scene::ActionRole::None?a.direction:scene::Direction::Any),int(a.weapon),a.ads?1:0};}
    static bool validSlot(const Slot& s){return std::get<0>(s)>=0&&std::get<0>(s)<=int(scene::ActionRole::Inspect)&&std::get<0>(s)!=int(scene::ActionRole::Death)&&std::get<1>(s)>=0&&std::get<1>(s)<=int(scene::MotionRole::Ladder)&&std::get<2>(s)>=0&&std::get<2>(s)<=int(scene::Stance::Prone)&&std::get<3>(s)>=0&&std::get<3>(s)<=int(scene::Direction::BackwardRight)&&std::get<4>(s)>=0&&std::get<4>(s)<=int(scene::WeaponClass::RC)&&std::get<5>(s)>=-1&&std::get<5>(s)<=1&&(std::get<0>(s)!=0||std::get<1>(s)!=0);}
    static int match(const Slot& s,const scene::AnimationQuery& q){
        if(std::get<0>(s)!=int(q.action)||(q.action==scene::ActionRole::None&&std::get<1>(s)!=int(q.motion)))return -1;
        int score=0;for(const auto [value,query]:{std::pair{std::get<2>(s),int(q.stance)},std::pair{std::get<3>(s),int(q.direction)},std::pair{std::get<4>(s),int(q.weapon)}}){if(value&&value!=query)return -1;score+=value?2:0;}
        if(std::get<5>(s)>=0){if(std::get<5>(s)!=int(q.ads))return -1;++score;}return score;
    }
    std::map<Key,bool> requested()const{auto result=rules;for(const auto& [slot,key]:overrides)result[key]=true;return result;}
    bool save(const std::filesystem::path& path,std::string* error=nullptr)const{
        std::string detail;const auto fail=[&](const std::string& message){if(error)*error=message;return false;};
        if(error)error->clear();
        if(path.empty()||path.filename().empty())return fail("Choose a preset filename");
        for(const auto& [slot,key]:overrides)if(!validSlot(slot)||std::get<0>(key).empty()||std::get<2>(key).empty()||std::get<0>(key).find_first_of("\r\n")!=std::string::npos||std::get<2>(key).find_first_of("\r\n")!=std::string::npos||std::get<1>(key)<0||std::get<1>(key)>int(scene::ActionRole::Inspect))return fail("Invalid animation assignment");
        for(const auto& [game,model]:models)if(game.empty()||model.empty()||game.find_first_of("\r\n")!=std::string::npos||model.find_first_of("\r\n")!=std::string::npos)return fail("Invalid model reference");
        for(const auto& [key,value]:rules)if(std::get<0>(key).empty()||std::get<2>(key).empty()||std::get<1>(key)<0||std::get<1>(key)>static_cast<int>(scene::ActionRole::Inspect)||std::get<0>(key).find_first_of("\r\n")!=std::string::npos||std::get<2>(key).find_first_of("\r\n")!=std::string::npos)return fail("Invalid animation rule");
        std::error_code ec;if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path(),ec);if(ec)return fail("Could not create preset folder: "+ec.message());
        const bool saved=take::detail::atomicFile(path,detail,[&](const auto& temporary){std::ofstream out(temporary);if(!out)return false;
        out<<"CADENCE_ANIMATION_SET "<<(overrides.empty()?1:2)<<"\ncurated_deaths "<<curatedDeaths<<'\n';
        for(const auto& [slot,key]:overrides)out<<"override "<<std::get<0>(slot)<<' '<<std::get<1>(slot)<<' '<<std::get<2>(slot)<<' '<<std::get<3>(slot)<<' '<<std::get<4>(slot)<<' '<<std::get<5>(slot)<<' '<<std::quoted(std::get<0>(key))<<' '<<std::get<1>(key)<<' '<<std::quoted(std::get<2>(key))<<'\n';
        for(const auto& [game,model]:models)out<<"model "<<std::quoted(game)<<' '<<std::quoted(model)<<'\n';
        for(const auto& [key,value]:rules)out<<"clip "<<std::quoted(std::get<0>(key))<<' '<<std::get<1>(key)<<' '<<std::quoted(std::get<2>(key))<<' '<<value<<'\n';out.flush();if(!out)return false;out.close();return bool(out);});
        if(!saved)return fail("Could not save preset; original preserved: "+detail);return true;
    }
    bool load(const std::filesystem::path& path,std::string* error=nullptr){
        if(error)error->clear();const auto fail=[&](const std::string& message){if(error)*error=message;return false;};
        std::ifstream in(path);if(!in)return fail("Could not open animation preset");std::string line,magic;int version{};
        if(!std::getline(in,line))return fail("Empty animation preset");std::istringstream header(line);
        if(!(header>>magic>>version)||magic!="CADENCE_ANIMATION_SET"||(version!=1&&version!=2)||!(header>>std::ws).eof())return fail("Unsupported animation preset");
        AnimationSet staged;
        std::size_t lineNumber=1;while(std::getline(in,line)){++lineNumber;std::istringstream row(line);std::string key;if(!(row>>key))continue;
        const auto invalid=[&]{return fail("Invalid animation preset at line "+std::to_string(lineNumber));};
        if(key=="curated_deaths"){if(!(row>>staged.curatedDeaths))return invalid();}else if(key=="model"){
            std::string game,model;if(!(row>>std::quoted(game)>>std::quoted(model))||game.empty()||model.empty())return invalid();staged.models[game]=model;
        }else if(key=="clip"){
            std::string game,name;int action{};bool value{};if(!(row>>std::quoted(game)>>action>>std::quoted(name)>>value)||action<0||action>static_cast<int>(scene::ActionRole::Inspect)||game.empty()||name.empty()||file(name).empty())return invalid();
            staged.set(game,static_cast<scene::ActionRole>(action),name,value);
        }else if(key=="override"&&version==2){Slot slot;Key clip;
            if(!(row>>std::get<0>(slot)>>std::get<1>(slot)>>std::get<2>(slot)>>std::get<3>(slot)>>std::get<4>(slot)>>std::get<5>(slot)>>std::quoted(std::get<0>(clip))>>std::get<1>(clip)>>std::quoted(std::get<2>(clip)))||!validSlot(slot)||std::get<0>(clip).empty()||file(std::get<2>(clip)).empty()||std::get<1>(clip)<0||std::get<1>(clip)>int(scene::ActionRole::Inspect))return invalid();std::get<2>(clip)=file(std::get<2>(clip));staged.overrides[slot]=clip;
        }else return invalid();if(!(row>>std::ws).eof())return invalid();}
        if(!in.eof())return fail("Could not read complete animation preset");*this=std::move(staged);return true;
    }
};
}
