#pragma once
#include "scene/CastScene.h"
namespace scene {
// Recompute only after clothing assembly, before world weapons are attached.
inline void refreshCharacterBounds(CastScene& actor){
 Bounds b;
 for(const auto& mesh:actor.meshes){if(mesh.attachmentIndex>=0)continue;for(const auto& v:mesh.vertices){const auto p=transformPoint(mesh.modelTransform,v.position);if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))continue;
  if(!b.valid){b.minimum=b.maximum=p;b.valid=true;}else{b.minimum={std::min(b.minimum.x,p.x),std::min(b.minimum.y,p.y),std::min(b.minimum.z,p.z)};b.maximum={std::max(b.maximum.x,p.x),std::max(b.maximum.y,p.y),std::max(b.maximum.z,p.z)};}
 }}if(b.valid)actor.bounds=b;
}
}
