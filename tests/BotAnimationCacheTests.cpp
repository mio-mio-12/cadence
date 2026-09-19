#include "app/BotAnimationQueryKey.h"
#include <iostream>
#include <stdexcept>

void require(bool value) { if(!value) throw std::runtime_error("bot animation cache regression"); }
int main() {
    scene::AnimationQuery base;
    std::vector<scene::AnimationQuery> queries{base};
    const auto add=[&](auto change){auto q=base;change(q);require(bot_animation::queryKey(q)!=bot_animation::queryKey(base));queries.push_back(q);};
    add([](auto& q){q.domain=scene::AnimationDomain::PlayerTorso;});
    add([](auto& q){q.motion=scene::MotionRole::Run;});
    add([](auto& q){q.action=scene::ActionRole::Fire;});
    add([](auto& q){q.weapon=scene::WeaponClass::Rifle;});
    add([](auto& q){q.stance=scene::Stance::Crouch;});
    add([](auto& q){q.direction=scene::Direction::Left;});
    add([](auto& q){q.ads=true;});
    add([](auto& q){q.reloadStyle=scene::ReloadStyle::RearClip;});
    add([](auto& q){q.allowContextual=true;});
    add([](auto& q){q.requireMappedTracks=false;});
    add([](auto& q){q.preferredGame="aw";});
    add([](auto& q){q.forceT6Locomotion=true;});
    scene::CastScene actor;
    for(int i=0;i<64;++i) {
        scene::Animation clip;
        clip.domain=i%2?scene::AnimationDomain::PlayerBody:scene::AnimationDomain::PlayerTorso;
        clip.action=i%3?scene::ActionRole::None:scene::ActionRole::Fire;
        clip.motion=i%2?scene::MotionRole::Idle:scene::MotionRole::Run;
        clip.weapon=i%2?scene::WeaponClass::Any:scene::WeaponClass::Rifle;
        clip.stance=scene::Stance::Stand;clip.ads=i%4==0;
        clip.contextual=i%5==0;clip.sourceGame=i%3?"bo2":"aw";
        if(i%7)clip.tracks.resize(1);
        actor.animations.push_back(clip);
    }
    bot_animation::ActionCache cache;
    for(int repeat=0;repeat<100;++repeat)for(const auto& q:queries)
        require(cache.find(actor,q)==scene::findBestAnimation(actor,q));
    require(cache.misses==queries.size());require(cache.hits==99*queries.size());
    std::cout<<"query equivalence: "<<queries.size()*100<<" selections, "<<cache.misses<<" scans, "<<cache.hits<<" cache hits\n";
    auto other=actor;other.animations.clear();other.animations.resize(actor.animations.size());
    require(cache.find(other,base)==scene::findBestAnimation(other,base));require(cache.misses==1);
    other.animations.push_back(actor.animations[1]);
    require(cache.find(other,base)==scene::findBestAnimation(other,base));require(cache.misses==1);
    // Same-address, same-count replacement needs the explicit rebuild reset.
    other.animations=actor.animations;cache.clear();
    require(cache.find(other,base)==scene::findBestAnimation(other,base));require(cache.misses==1);
    std::vector<scene::PoseSlot> scratch(3);
    for(auto& slot:scratch)slot.nodes.reserve(1);
    const auto capacity=scratch[0].nodes.capacity();
    for(int frame=0;frame<100;++frame)for(int bot=0;bot<18;++bot) {
        for(auto& slot:scratch)slot.nodes.clear();
        if((frame+bot)%2)scratch[0].nodes.push_back({0,0,1,scene::LayerMode::Override,false});
        require(scratch[0].nodes.size()==static_cast<std::size_t>((frame+bot)%2));
        require(scratch[0].nodes.capacity()==capacity);
        require(scratch[1].nodes.empty()&&scratch[2].nodes.empty());
    }
    std::cout<<"all 12 query fields distinct; scene/rebuild invalidation passed; 1800 scratch uses retained capacity\n";
    scene::CastScene postures;
    for(const auto* name:{"pt_rifle_fire","pt_rifle_fire_prone","pt_rifle_prone_reload_rearclip","pt_rifle_reload_rearclip"}) {
        scene::Animation clip;scene::classifyAnimationName(name,clip);clip.tracks.resize(1);
        postures.animations.push_back(clip);
    }
    const auto proneFire=bot_animation::actionQuery(scene::ActionRole::Fire,scene::WeaponClass::Rifle,scene::Stance::Prone);
    const auto standFire=bot_animation::actionQuery(scene::ActionRole::Fire,scene::WeaponClass::Rifle,scene::Stance::Stand);
    const auto proneReload=bot_animation::actionQuery(scene::ActionRole::Reload,scene::WeaponClass::Rifle,scene::Stance::Prone);
    for(const auto& q:{proneFire,standFire,proneReload}) {
        const auto selected=cache.find(postures,q);
        std::cerr<<"posture regression query "<<static_cast<int>(q.action)<<" "<<static_cast<int>(q.stance)<<" selected "<<(selected?int(*selected):-1)<<'\n';
    }
    require(cache.find(postures,proneFire)==1);require(cache.find(postures,standFire)==0);require(cache.find(postures,proneReload)==2);
    require(!bot_animation::postureCompatible(postures.animations[0],scene::Stance::Prone));
    require(!bot_animation::postureCompatible(postures.animations[1],scene::Stance::Stand));
    require(bot_animation::postureCompatible(postures.animations[1],scene::Stance::Prone));
    postures.animations.erase(postures.animations.begin()+1);
    require(!cache.find(postures,proneFire)); // Missing prone action must not use a standing substitute.
    std::cout<<"prone fire/reload selection, stance-change guard, and missing prone action passed\n";
}
