#include "app/BotAnimationQueryKey.h"
#include "scene/PointBlankActionIdentity.h"
#include <iostream>
#include <stdexcept>

void require(bool value) { if(!value) throw std::runtime_error("bot animation cache regression"); }
int main() {
    using scene::pointblank::meleeFamilyFromViewmodel;
    using scene::pointblank::torsoWeaponFamily;
    using scene::pointblank::torsoFamilyCompatible;
    require(meleeFamilyFromViewmodel("D:/pointblank/models/viewmodel_knife_M-9.cast")=="knife");
    require(meleeFamilyFromViewmodel("D:\\pointblank\\models\\VIEWMODEL_KNIFE_M-9_DUAL.CAST")=="dualknife");
    require(meleeFamilyFromViewmodel("viewmodel_knife_Spetnatz_Field_Shovel.cast")=="spetnatz_field_shovel");
    require(meleeFamilyFromViewmodel("viewmodel_pistol_DesertEagle.cast").empty());
    require(meleeFamilyFromViewmodel("viewmodel_SWAT_Male_hands.cast").empty());
    require(meleeFamilyFromViewmodel("worldmodel_knife_M-9.cast").empty());
    // Additional native families exist in torso exports; these viewmodel names
    // are synthetic convention checks, not claims that those models are loaded.
    for(const auto family:scene::pointblank::meleeFamilies){
        const auto clip="pt_SWAT_Female_GRS_"+std::string(family)+"_Common_Up_AttackIdle.cast";
        require(torsoWeaponFamily(clip)==family);
        require(torsoFamilyCompatible(family,"pointblank",clip));
        require(meleeFamilyFromViewmodel("viewmodel_"+std::string(family)+".cast")==family);
        if(family!="knife")require(!torsoFamilyCompatible("knife","pointblank",clip));
    }
    require(torsoWeaponFamily("pt_REBEL_Male_knife_Common_Up_Attack.cast")=="knife");
    require(torsoWeaponFamily("pt_SWAT_Female_GRS_brass_knuckle_Common_Up_AttackIdle.cast")=="brass_knuckle");
    require(torsoWeaponFamily("pt_SWAT_Female_GRS_dualknife_Common_Up_Attack.cast")=="dualknife");
    require(torsoWeaponFamily("pt_SWAT_Female_GRS_dualknife_Dual_Throwing_Knife_Up_Attack_A.cast")=="dualknife");
    require(!torsoFamilyCompatible("knife","pointblank","pt_SWAT_Female_GRS_dualknife_Dual_Throwing_Knife_Up_Attack_A.cast"));
    require(torsoWeaponFamily("pt_SWAT_Female_GRS_dualknife_Kunai_Dual_Up_Attack_A.cast")=="dualknife");
    require(torsoWeaponFamily("pt_SWAT_Female_GRS_knife_Butterfly_Knife_Up_Attack_A.cast")=="knife");
    require(torsoWeaponFamily("pt_SWAT_Female_GRS_kukrii_Arabian_sword_Up_Attack_A.cast")=="kukrii");
    for(const auto descriptor:{"CandyCane","Ice_Fork","Stick_Goat"})require(torsoWeaponFamily("pt_SWAT_Female_GRS_keris_"+std::string(descriptor)+"_Up_Attack_A.cast")=="keris");
    require(torsoWeaponFamily("pt_SWAT_Female_GRS_gh5007_M1918_TrenchKnife_Up_Attack_A.cast").empty());
    require(torsoWeaponFamily("pt_SWAT_Female_GRS_knife_Common_Up_").empty());
    require(torsoWeaponFamily("pt_SWAT_Female_GRS_notknife_Common_Up_Attack.cast").empty());
    require(torsoWeaponFamily("pb_SWAT_Female_GRS_knife_Common_Up_Attack.cast").empty());
    require(!torsoFamilyCompatible("knife","pointblank","pt_SWAT_Female_GRS_unknown_Common_Up_Attack.cast"));
    require(torsoFamilyCompatible("knife","bo2","pt_knife_idle.cast"));
    require(torsoFamilyCompatible("","pointblank","pt_SWAT_Female_GRS_brass_knuckle_Common_Up_AttackIdle.cast"));
    std::cout<<"PB native melee family identity, specific grips, gender independence and foreign fallback passed\n";
    scene::AnimationQuery base;
    {scene::CastScene variants;for(const auto descriptor:{"BallisticKnife","Butterfly_Knife","Common"}){scene::Animation c;c.sourceName="pt_SWAT_Female_GRS_knife_"+std::string(descriptor)+"_Up_Attack.cast";c.sourceGame="pointblank";c.domain=scene::AnimationDomain::PlayerTorso;c.action=scene::ActionRole::Melee;c.weapon=scene::WeaponClass::Knife;c.tracks.resize(20);variants.animations.push_back(c);}
     auto q=bot_animation::actionQuery(scene::ActionRole::Melee,scene::WeaponClass::Knife,scene::Stance::Stand);q.preferredWeaponFamily="knife";require(scene::findBestAnimation(variants,q)==2);
     variants.animations.pop_back();require(scene::findBestAnimation(variants,q)==0);
     variants.animations.resize(1);variants.animations[0].sourceName="pt_SWAT_Female_GRS_spetnatz_field_shovel_Spetnatz_Field_Shovel_Up_Attack_A.cast";q.preferredWeaponFamily="spetnatz_field_shovel";require(scene::findBestAnimation(variants,q)==0);}
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
    add([](auto& q){q.preferredWeaponFamily="knife";});
    {scene::CastScene melee;
     for(const auto family:{"brass_knuckle","knife","dualknife"}){scene::Animation c;c.sourceName="pt_SWAT_Female_GRS_"+std::string(family)+"_Common_Up_Attack.cast";c.sourceGame="pointblank";c.domain=scene::AnimationDomain::PlayerTorso;c.action=scene::ActionRole::Melee;c.weapon=scene::WeaponClass::Knife;c.tracks.resize(1);melee.animations.push_back(c);}
     auto q=bot_animation::actionQuery(scene::ActionRole::Melee,scene::WeaponClass::Knife,scene::Stance::Stand);bot_animation::ActionCache identities;
     q.preferredWeaponFamily="knife";require(identities.find(melee,q)==1);
     q.preferredWeaponFamily="dualknife";require(identities.find(melee,q)==2);
     q.preferredWeaponFamily="brass_knuckle";require(identities.find(melee,q)==0);
     q.preferredWeaponFamily="spetnatz_field_shovel";require(!identities.find(melee,q));
     auto fallback=melee.animations[0];fallback.sourceGame="bo2";fallback.sourceName="pt_knife_stand_melee_swipe.cast";melee.animations.push_back(fallback);require(identities.find(melee,q)==3);}
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
    std::cout<<"all 13 query fields distinct; scene/rebuild invalidation passed; 1800 scratch uses retained capacity\n";
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
