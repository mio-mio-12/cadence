#pragma once
#include "take/Take.h"
#include "scene/CastScene.h"
namespace cadence {
inline bool equipmentAction(scene::ActionRole role){
    return role==scene::ActionRole::Throw||role==scene::ActionRole::GrenadePrep||role==scene::ActionRole::Deploy||role==scene::ActionRole::Plant;
}
inline std::uint8_t actorVisibility(bool equipmentHidden,bool adsEngaged,float ads,bool hideOnAds,bool sniper){
    const bool hidden=adsEngaged&&ads>=.999f&&hideOnAds;
    return take::VisibilityKnown|(equipmentHidden?take::HideWeapon:0)|(hidden?take::HideViewmodel:0)|(hidden&&sniper?take::ScopeOverlay:0);
}
// Recorded perspective is authoritative. Live free-camera/shoulder flags must
// not leak into a take, including while playback is paused.
inline bool showPlayerWorldProxy(bool replay,bool firstPerson,bool editor,bool actor,bool shoulder,bool freecam){
    if(replay)return !firstPerson;
    return (editor&&!firstPerson)||(actor&&(shoulder||freecam));
}
inline bool showFirstPersonRig(bool replay,bool firstPerson,bool hidden,bool actor,bool shoulder,bool worldProxy,bool retainViewmodel,bool navigation){
    if(replay)return firstPerson&&!hidden;
    return !hidden&&!(actor&&shoulder)&&(!worldProxy||retainViewmodel)&&!navigation;
}
inline bool useViewmodelCamera(bool replay,bool firstPerson,bool viewmodel,bool actor,bool shoulder,bool retainViewmodel){
    if(replay)return firstPerson;
    return (viewmodel||actor)&&!(actor&&shoulder)&&!retainViewmodel;
}
}
