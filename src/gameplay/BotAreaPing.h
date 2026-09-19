#pragma once
#include "gameplay/BotActor.h"

namespace gameplay::bot {
// Session-only intention. Recorded actor poses remain the replay authority.
struct AreaPing {
    scene::Vec3 point{};
    float remaining{};
    std::uint32_t sequence{};
    std::vector<std::uint32_t> recipients;
    bool contains(std::uint32_t id)const{return remaining>0&&std::find(recipients.begin(),recipients.end(),id)!=recipients.end();}
    void remove(std::uint32_t id){std::erase(recipients,id);}
    void clear(){remaining=0;recipients.clear();}
    void set(scene::Vec3 destination,float percent,const std::vector<Actor>& bots){
        clear();point=destination;++sequence;
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z)||!std::isfinite(percent))return;
        for(const auto& bot:bots)if(bot.alive)recipients.push_back(bot.id);
        const auto rank=[&](std::uint32_t id){std::uint32_t v=id^(sequence*0x9e3779b9u);v^=v>>16;v*=0x7feb352du;v^=v>>15;return v;};
        std::sort(recipients.begin(),recipients.end(),[&](auto a,auto b){return rank(a)==rank(b)?a<b:rank(a)<rank(b);});
        recipients.resize(static_cast<size_t>(std::lround(recipients.size()*std::clamp(percent,0.f,100.f)/100.f)));
        remaining=recipients.empty()?0.f:20.f;
    }
    void advance(float delta,const std::vector<Actor>& bots){
        remaining=std::max(0.f,remaining-std::max(0.f,delta));
        if(remaining<=0){clear();return;}
        std::erase_if(recipients,[&](auto id){auto b=std::find_if(bots.begin(),bots.end(),[&](const auto& b){return b.id==id;});return b==bots.end()||!b->alive||(horizontalDistance(b->position,point)<100.f&&std::abs(b->position.z-point.z)<65.f);});
    }
};
}
