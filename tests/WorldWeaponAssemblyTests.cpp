#include "app/WorldWeaponAssembly.h"
#include <iostream>
int main(){using namespace cadence::world_weapon;int failures=0;
 const auto check=[&](bool condition,const char* message){if(!condition){std::cerr<<message<<'\n';++failures;}};
 check(modelIdentity("t6_wpn_sniper_ballista_scope_world_LOD0")==modelIdentity("t6_wpn_sniper_ballista_scope_view_LOD0"),"Exact T6 counterpart");
 check(modelIdentity("vm_mors_base_scope_LOD0")==modelIdentity("npc_mors_base_scope_LOD0"),"Exact AW counterpart");
 check(modelIdentity("vm_mors_base_scope_LOD0")!=modelIdentity("npc_mors_accuracy_scope_LOD0"),"Do not borrow variant");
 check(detachedAwBotModule("vm_bal27_damage_base_LOD0")&&!detachedAwBotModule("vm_bal27_base_standard_LOD0"),"Detached AW module excluded without excluding complete base");
 check(detachedAwBotModule("vm_bal27_mobility_base_LOD0")&&detachedAwBotModule("vm_bal27_range_base_LOD0"),"Stock/muzzle modules are not standalone bot rifles");
 check(preferAvailableWorldModels({1,2,3},[](auto i){return i==2;})==std::vector<std::size_t>{2},"Automatic pool prefers actual world models");
 check(preferAvailableWorldModels({1,2,3},[](auto){return false;})==std::vector<std::size_t>({1,2,3}),"All-missing native pool retains honest diagnostics");
 check(weaponFields("WRONG\\key\\value").empty(),"Reject wrong format");
 check(weaponFields("WEAPONFILE\\worldModel\\a\\worldModel\\b").empty(),"Reject duplicate placement fields");
 Fields f;for(const auto axis:{"X","Y","Z"})f[std::string("attachWorldModelOffset1")+axis]="0";
 for(const auto axis:{"Pitch","Yaw","Roll"})f[std::string("attachWorldModelRotation1")+axis]="0";
 f["attachWorldModelOffset1X"]="-5.696";auto station=t6Station(f,"tag_scope");
 check(station&&std::abs(station->v[12]+14.46784f)<.0001f,"Source inches converted once");
 f["attachWorldModelOffset1X"]="nan";check(!t6Station(f,"tag_scope"),"Reject nonfinite");
 f["attachWorldModelOffset1X"]="1oops";check(!t6Station(f,"tag_scope"),"Reject partial number");
 scene::CastScene base,part;scene::Bone socket;socket.name="tag_scope";socket.restGlobal=scene::translation({3,4,5});base.skeleton.bones.push_back(socket);base.skeleton.boneByCanonicalName["tag_scope"]=0;
 socket.parent=-1;socket.restGlobal=scene::translation({1,0,0});part.skeleton.bones.push_back(socket);
 auto mounted=socketTransform(base,part);check(mounted&&mounted->v[12]==2&&mounted->v[13]==4,"Exact authored socket root correction");
 base.skeleton.boneByCanonicalName.clear();check(!socketTransform(base,part),"Missing socket does not guess");
 scene::Attachment a;const auto matrix=scene::trs({1,2,3},scene::fromEulerRadians({.2f,-.3f,.4f}),{1,1,1});setTransform(a,matrix);for(int i=0;i<16;++i)check(std::abs(a.localMatrix().v[i]-matrix.v[i])<.0001f,"Replay TRS preserves matrix");
 return failures?1:0;
}
