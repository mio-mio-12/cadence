#pragma once
#include "scene/Math3D.h"
#include <algorithm>
#include <deque>
#include <unordered_map>
#include <vector>
#include <cstdint>
namespace render {
struct ActorOverlaySettings {
 bool chams{},throughWalls{},outlines{},glow{},echoes{},ribbons{},arcs{},landingRings{},hitGhosts{},impactPulses{},transparentWorld{},boxes{},rainbow{},impactCubes{};
 int material{},echoCount{3};
 float opacity{.65f},outlineWidth{2},glowWidth{7},glowStrength{.3f},historySeconds{1},historySpacing{.1f},lineWidth{2},ringRadius{35},hitDuration{.5f},worldOpacity{.35f},boxWidth{1.5f},rainbowSpeed{.15f},cubeSize{3},cubeDuration{1};
 scene::Vec4 visible{.1f,.8f,1,1},hidden{1,.15f,.5f,.65f},edge{.2f,.9f,1,1},motion{.25f,.8f,1,.7f},hit{1,.35f,.1f,1},box{.2f,1,.5f,1};
 bool historyEnabled()const{return echoes||ribbons||arcs||landingRings||hitGhosts||impactPulses;}
 void sanitize(){
  auto clamp=[](float& v,float lo,float hi,float fallback){v=std::isfinite(v)?std::clamp(v,lo,hi):fallback;};
  material=std::clamp(material,0,3);echoCount=std::clamp(echoCount,1,6);
  clamp(opacity,0,1,.65f);clamp(outlineWidth,.5f,8,2);clamp(glowWidth,1,20,7);clamp(glowStrength,0,1,.3f);
  clamp(historySeconds,.1f,3,1);clamp(historySpacing,.05f,.5f,.1f);clamp(lineWidth,.5f,8,2);
  clamp(ringRadius,1,150,35);clamp(hitDuration,.05f,2,.5f);clamp(worldOpacity,0,1,.35f);clamp(boxWidth,.5f,8,1.5f);
  clamp(rainbowSpeed,0,2,.15f);clamp(cubeSize,.1f,30,3);clamp(cubeDuration,.05f,5,1);
  for(auto* c:{&visible,&hidden,&edge,&motion,&hit,&box})for(auto* x:{&c->x,&c->y,&c->z,&c->w})clamp(*x,0,1,1);
 }
};
struct ActorOverlayInput {std::uint64_t id{};float health{100};bool airborne{},sliding{};};
struct ActorTrailSample {double time{};scene::Vec3 root{};bool airborne{},sliding{};std::vector<scene::Mat4> pose;};
struct ActorTrailState {
 std::deque<ActorTrailSample> samples;
 std::vector<scene::Mat4> hitPose;
 double lastSeen{},hitTime{-100},landingTime{-100};
 scene::Vec3 landing{};
 float health{100};bool airborne{},initialized{};
};
class ActorOverlayHistory {
public:
 std::unordered_map<std::uint64_t,ActorTrailState> actors;
 double time{-1};
 void begin(double now,bool enabled){
  if(!enabled||!std::isfinite(now)||now<time||now-time>.5){actors.clear();}
  time=now;
  for(auto i=actors.begin();i!=actors.end();)if(now-i->second.lastSeen>3.1)i=actors.erase(i);else ++i;
 }
 ActorTrailState* update(const ActorOverlayInput& input,const std::vector<scene::Mat4>& pose,scene::Vec3 root,const ActorOverlaySettings& settings){
  if(!settings.historyEnabled()||pose.empty()||!std::isfinite(root.x)||!std::isfinite(root.y)||!std::isfinite(root.z))return nullptr;
  if(!actors.contains(input.id)&&actors.size()>=64)return nullptr;
  auto& h=actors[input.id];
  if(h.initialized&&!h.samples.empty()&&scene::length(root-h.samples.back().root)>500)h={};
  if(h.initialized&&input.health<h.health-.01f){h.hitTime=time;if(settings.hitGhosts)h.hitPose=pose;}
  if(h.initialized&&h.airborne&&!input.airborne){h.landingTime=time;h.landing=root;}
  h.health=input.health;h.airborne=input.airborne;h.lastSeen=time;h.initialized=true;
  while(!h.samples.empty()&&(time-h.samples.front().time>settings.historySeconds||h.samples.size()>=64))h.samples.pop_front();
  if(h.samples.empty()||time-h.samples.back().time>=settings.historySpacing-.00001){
   ActorTrailSample sample;sample.time=time;sample.root=root;sample.airborne=input.airborne;sample.sliding=input.sliding;
   if(settings.echoes&&(input.airborne||input.sliding))sample.pose=pose;
   h.samples.push_back(std::move(sample));
  }
  if(time-h.hitTime>settings.hitDuration)h.hitPose.clear();
  return &h;
 }
};
}

