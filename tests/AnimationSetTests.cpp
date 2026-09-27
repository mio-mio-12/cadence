#include "app/AnimationSet.h"
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
    for(const auto* invalid:{"", "CADENCE_ANIMATION_SET 2\n", "CADENCE_ANIMATION_SET 1 junk\n", "CADENCE_ANIMATION_SET 1\nclip", "CADENCE_ANIMATION_SET 1\nclip \"bo2\" 6 \"a.cast\"", "CADENCE_ANIMATION_SET 1\nmodel \"bo2\"", "CADENCE_ANIMATION_SET 1\ncurated_deaths", "CADENCE_ANIMATION_SET 1\nclip \"bo2\" 6 \"a.cast\" 2", "CADENCE_ANIMATION_SET 1\ncurated_deaths 1 trailing", "CADENCE_ANIMATION_SET 1\nunknown 1"}){
        {std::ofstream out(file);out<<invalid;}std::string error;check(!round.load(file,&error));check(!error.empty());check(round==set);
    }
    check(set.save(file));auto invalid=set;invalid.models["bad"]="line\nbreak";check(!invalid.save(file));check(round.load(file)&&round==set);
    const auto directory=std::filesystem::path(file.string()+".directory");std::filesystem::create_directory(directory);std::string saveError;check(!set.save(directory,&saveError));check(!saveError.empty());check(std::filesystem::is_directory(directory));std::filesystem::remove(directory);
    std::filesystem::remove(file);check(!round.load(file));check(round==set);
    std::mt19937 a(17),b(17);check(cadence::randomizedVisual(200,0,1,100,a)==200);check(cadence::randomizedVisual(200,0,1,0,a)==cadence::randomizedVisual(-10,0,1,0,b));
    for(int i=0;i<1000;++i){const float v=cadence::randomizedVisual(.5f,0,1,90,a);check(v>=.45f&&v<=.55f);}
    {cadence::VisualRandomBaseline base;float v=2.f;bool flag=true;check(base.original(v)==2.f);check(base.original(flag));
        float first{};for(int seed:{41,7,41}){std::mt19937 rng(seed);v=cadence::randomizedVisual(base.original(v),0,10,75,rng);if(!first)first=v;else if(seed==41)check(v==first);}
        flag=false;base.restore();check(v==2.f&&flag);base.clear();v=5;check(base.original(v)==5);}
    if(errors)std::cerr<<errors<<" failures\n";return errors?1:0;
}
