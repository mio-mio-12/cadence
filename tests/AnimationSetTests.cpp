#include "app/AnimationSet.h"
#include "app/AnimationSetRuntime.h"
#include "app/VisualRandomizer.h"
#include "assets/ImportedGamePolicy.h"
#include <iostream>
int main(){int errors{};const auto check=[&](bool pass){if(!pass)++errors;};cadence::AnimationSet set;
    check(assets::imported::dewBodySemantic("mp_masterchief_6262_combat_any_airborne_dead_88.cast")=="pb_generic_stand_death");
    check(assets::imported::dewBodySemantic("", "combat:any:landing_dead")=="pb_generic_stand_death");
    check(assets::imported::dewBodySemantic("mp_elite_26901_combat_landing_dead_247.cast")=="pb_generic_stand_death");
    check(set.enabled("bo2",scene::ActionRole::Death,"a.cast"));set.set("bo2",scene::ActionRole::Death,"a.cast",false);
    check(!set.enabled("bo2",scene::ActionRole::Death,"a.cast"));check(set.enabled("css",scene::ActionRole::Death,"a.cast"));
    set.curatedDeaths=true;check(!set.enabled("css",scene::ActionRole::Death,"a.cast"));check(set.enabled("bo2",scene::ActionRole::Fire,"a.cast"));set.set("css",scene::ActionRole::Death,"a.cast",true);
    set.models["css"]="ct_sas";
    const auto file=std::filesystem::temp_directory_path()/"cadence_animation_set_test.cfg";check(set.save(file));cadence::AnimationSet round;check(round.load(file));check(round.rules==set.rules&&round.curatedDeaths&&round.models==set.models);
    {std::ofstream out(file);out<<"CADENCE_ANIMATION_SET 1\nclip \"bo2\" 999 \"a.cast\" 1\n";}check(!round.load(file));check(round.rules==set.rules);
    for(int action=0;action<=static_cast<int>(scene::ActionRole::Inspect);++action){set.set("bo2",static_cast<scene::ActionRole>(action),"clip with spaces.cast",action%2==0);set.set("css",static_cast<scene::ActionRole>(action),"clip with spaces.cast",action%2!=0);}
    set.models["bo2"]="character \"quoted\" name";check(set.save(file));check(round.load(file));check(round==set);
    for(const auto* invalid:{"", "CADENCE_ANIMATION_SET 3\n", "CADENCE_ANIMATION_SET 1 junk\n", "CADENCE_ANIMATION_SET 1\nclip", "CADENCE_ANIMATION_SET 1\nclip \"bo2\" 6 \"a.cast\"", "CADENCE_ANIMATION_SET 1\nmodel \"bo2\"", "CADENCE_ANIMATION_SET 1\ncurated_deaths", "CADENCE_ANIMATION_SET 1\nclip \"bo2\" 6 \"a.cast\" 2", "CADENCE_ANIMATION_SET 1\ncurated_deaths 1 trailing", "CADENCE_ANIMATION_SET 1\nunknown 1", "CADENCE_ANIMATION_SET 2\noverride 0 99 0 0 0 -1 \"bo2\" 0 \"x.cast\""}){
        {std::ofstream out(file);out<<invalid;}std::string error;check(!round.load(file,&error));check(!error.empty());check(round==set);
    }
    check(set.save(file));auto invalid=set;invalid.models["bad"]="line\nbreak";check(!invalid.save(file));check(round.load(file)&&round==set);
    const auto directory=std::filesystem::path(file.string()+".directory");std::filesystem::create_directory(directory);std::string saveError;check(!set.save(directory,&saveError));check(!saveError.empty());check(std::filesystem::is_directory(directory));std::filesystem::remove(directory);
    std::filesystem::remove(file);check(!round.load(file));check(round==set);
    {
        cadence::AnimationSet partial;cadence::AnimationSet::Slot slot{0,int(scene::MotionRole::Run),int(scene::Stance::Stand),int(scene::Direction::Forward),0,-1};
        partial.overrides[slot]={"cso2",0,"run.cast"};partial.curatedDeaths=true;partial.set("bo2",scene::ActionRole::Death,"a.cast",true);partial.set("cso2",scene::ActionRole::Death,"a.cast",true);
        check(partial.save(file));cadence::AnimationSet loaded;check(loaded.load(file)&&loaded==partial);std::filesystem::remove(file);
        scene::CastScene actor;scene::Animation clip;clip.tracks.resize(1);clip.domain=scene::AnimationDomain::PlayerBody;clip.action=scene::ActionRole::Death;clip.sourceName="a.cast";clip.sourceGame="bo2";actor.animations.push_back(clip);clip.sourceGame="cso2";actor.animations.push_back(clip);
        clip.action=scene::ActionRole::None;clip.motion=scene::MotionRole::Run;clip.sourceName="base_run.cast";actor.animations.push_back(clip);
        cadence::AnimationSetRuntime runtime;runtime.begin(actor);clip.sourceName="run.cast";actor.animations.push_back(clip);runtime.bound[{"cso2",0,"run.cast"}]=3;runtime.bound[{"bo2",int(scene::ActionRole::Death),"a.cast"}]=0;runtime.bound[{"cso2",int(scene::ActionRole::Death),"a.cast"}]=1;runtime.refresh(actor,partial,true);
        check(runtime.deaths(actor,partial,true).size()==2);check(actor.animations[0].tracks.size()==1&&runtime.selection.meshes.empty()&&runtime.selection.skeleton.bones.empty());
        scene::AnimationQuery q;q.motion=scene::MotionRole::Run;q.direction=scene::Direction::Forward;check(runtime.find(q)==3);q.direction=scene::Direction::Backward;check(!runtime.find(q));q.motion=scene::MotionRole::Walk;check(!runtime.find(q));
        check(runtime.selection.animations[2].tracks.size()==1&&runtime.selection.animations[3].tracks.empty());
        runtime.refresh(actor,partial,false);check(runtime.overrides.empty()&&runtime.selection.animations[2].tracks.size()==1&&runtime.selection.animations[3].tracks.empty());
        partial.overrides[{int(scene::ActionRole::Fire),0,0,0,0,-1}]={"cso2",0,"run.cast"};
        partial.overrides[{int(scene::ActionRole::Fire),0,int(scene::Stance::Crouch),0,0,1}]={"bo2",int(scene::ActionRole::Death),"a.cast"};
        runtime.refresh(actor,partial,true);q={};q.action=scene::ActionRole::Fire;q.stance=scene::Stance::Stand;check(runtime.find(q)==3);
        q.stance=scene::Stance::Crouch;q.ads=true;check(runtime.find(q)==0);q.ads=false;check(runtime.find(q)==3);
        check(runtime.assignedPosture(0,scene::Stance::Crouch)&&!runtime.assignedPosture(0,scene::Stance::Stand));
        partial.overrides[{0,int(scene::MotionRole::Walk),0,0,0,-1}]={"missing",0,"absent.cast"};runtime.refresh(actor,partial,true);q={};q.motion=scene::MotionRole::Walk;check(!runtime.find(q));
        auto invalid=partial;std::get<5>(slot)=2;invalid.overrides[slot]={"cso2",0,"run.cast"};check(!invalid.save(file));
    }
    std::mt19937 a(17),b(17);check(cadence::randomizedVisual(200,0,1,100,a)==200);check(cadence::randomizedVisual(200,0,1,0,a)==cadence::randomizedVisual(-10,0,1,0,b));
    for(int i=0;i<1000;++i){const float v=cadence::randomizedVisual(.5f,0,1,90,a);check(v>=.45f&&v<=.55f);}
    {cadence::VisualRandomBaseline base;float v=2.f;bool flag=true;check(base.original(v)==2.f);check(base.original(flag));
        float first{};for(int seed:{41,7,41}){std::mt19937 rng(seed);v=cadence::randomizedVisual(base.original(v),0,10,75,rng);if(!first)first=v;else if(seed==41)check(v==first);}
        flag=false;base.restore();check(v==2.f&&flag);base.clear();v=5;check(base.original(v)==5);}
    if(errors)std::cerr<<errors<<" failures\n";return errors?1:0;
}
