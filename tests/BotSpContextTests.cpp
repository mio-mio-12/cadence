#include "app/BotSpContextLibrary.inl"
#include <filesystem>
#include <iostream>
#include <set>

int main(int argc,char** argv){
    using namespace cadence::sp;
    int failures=0;
    const auto check=[&](bool good,const char* message){if(!good){++failures;std::cerr<<message<<'\n';}};
    ContextLibrary library;
    std::size_t index=0;
    for(const auto& d:kContextDescriptors)library.push_back({index++,d});
    const auto picked=[&](ContextRequest r){const auto clip=selectContextClip(library,r,1,1);return clip?&library[*clip].descriptor:nullptr;};
    check(contextRigSupported("bo2")&&contextRigSupported("bo2_sp")&&contextRigSupported("mw3")&&!contextRigSupported("ghosts")&&!contextRigSupported("cs2"),"SP rig allowlist mismatch");
    for(auto weapon:{scene::WeaponClass::Rifle,scene::WeaponClass::Automatic,scene::WeaponClass::Sniper,scene::WeaponClass::Shotgun,scene::WeaponClass::M1216,scene::WeaponClass::G11,scene::WeaponClass::LMG,scene::WeaponClass::Crossbow})check(supportsSpLongGun(weapon),"existing generic rifle family excluded from SP");
    for(auto weapon:{scene::WeaponClass::Pistol,scene::WeaponClass::DualWield,scene::WeaponClass::RiotShield,scene::WeaponClass::Heavy,scene::WeaponClass::Minigun,scene::WeaponClass::Equipment,scene::WeaponClass::Any})check(!supportsSpLongGun(weapon),"incompatible weapon admitted to SP long-gun family");
    check(findContextDescriptor("stand_aim_up")!=nullptr,"verified stand pose missing");
    for(const char* excluded:{"ai_crew_m113_gunner_death","ai_balcony_death_01","ai_rappel_loop_in_place","ladder_climbup","death_explosion_stand_f_v2","ai_deadly_wounded_torso_die","invented_cover_idle","ai_wounded_exposed_idle","covercrouch_hide_idlea","covercrouch_hide_idleb"})check(!findContextDescriptor(excluded),"unsupported or visually rejected context accepted");
    check(kContextDescriptors.size()==32,"visual rejection shortlist unexpectedly changed");
    ContextRequest r;r.kind=ContextKind::CoverFire;r.firing=true;r.coverHeightIw=56;
    check(!picked(r),"cover fire allowed without geometry anchor");r.coverValidated=true;
    check(picked(r)&&picked(r)->kind==ContextKind::CoverFire,"matched standing cover fire missing");
    r.stance=ContextStance::Crouch;r.coverHeightIw=36;check(picked(r)&&picked(r)->stance==ContextStance::Crouch,"crouch cover mismatched posture");
    r.coverHeightIw=100;check(!picked(r),"cover at wrong height accepted");
    r={};r.kind=ContextKind::Death;check(!picked(r),"death selected on living actor");
    r.alive=false;r.justDied=true;check(picked(r)&&!picked(r)->moving,"standing death not matched");r.moving=true;check(picked(r)&&picked(r)->moving,"moving death not matched");
    r.justDied=false;check(!picked(r),"death randomly restarted on corpse");
    r={};r.kind=ContextKind::Throw;check(picked(r)!=nullptr,"standing animation-only throw missing");r.firing=true;check(!picked(r),"throw overrides fire");
    r={};r.kind=ContextKind::StandAim;r.aimPitchDegrees=40;check(picked(r)&&picked(r)->name=="stand_aim_up","upward aim mismatch");r.aimPitchDegrees=-40;check(picked(r)&&picked(r)->name=="stand_aim_down","downward aim mismatch");
    r={};r.kind=ContextKind::TraversalMantle;r.traversalHeightIw=49;check(!picked(r),"mantle animation without traversal geometry");r.traversalValidated=true;r.mantling=true;r.grounded=false;
    check(picked(r)&&picked(r)->name=="ai_mantle_on_48","nearest mantle-on height not chosen");
    r.mantleOver=true;r.traversalHeightIw=36;check(picked(r)&&picked(r)->name=="ai_mantle_over_36","mantle over mismatched mantle on");
    r={};r.kind=ContextKind::Jump;r.traversalValidated=true;r.traversalHeightIw=40;
    check(!picked(r),"drop animation selected for ordinary forward jump");r.jumpDown=true;check(picked(r)&&picked(r)->name=="ai_jump_down_40","validated drop height unmatched");
    r={};r.kind=ContextKind::WoundedWalk;r.moving=true;check(!picked(r),"permanent wounded movement without temporary episode");r.temporaryWounded=true;check(picked(r)!=nullptr,"temporary wounded movement absent");
    r.reloading=true;check(!picked(r),"wounded movement overrides reload ownership");
    r={};check(!selectContextClip(library,r,2,1,0),"zero context chance selected SP instead of MP fallback");
    check(selectContextClip(library,r,2,1,1)==selectContextClip(library,r,2,1,1),"context selection nondeterministic");
    if(argc>1){
        std::set<std::string> names;std::error_code error;
        for(std::filesystem::recursive_directory_iterator it(argv[1],error),end;it!=end;it.increment(error))if(!error&&it->is_regular_file()&&it->path().extension()==".cast")names.insert(it->path().stem().string());
        for(const auto& d:kContextDescriptors)if(!names.contains(std::string(d.name))){++failures;std::cerr<<"Absent ripped animation: "<<d.name<<'\n';}
        std::cout<<"Checked "<<kContextDescriptors.size()<<" exact names against "<<names.size()<<" ripped animation stems.\n";
    }
    if(!failures)std::cout<<"Context whitelist and stance/anchor/height/event/ownership selection PASS.\n";
    return failures?1:0;
}
