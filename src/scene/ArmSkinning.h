#pragma once
#include "scene/CastScene.h"
#include "scene/VolumePreservingFit.h"
namespace scene {
inline bool rigidArmTransform(const Mat4& m){
 const Vec3 x{m.v[0],m.v[1],m.v[2]},y{m.v[4],m.v[5],m.v[6]},z{m.v[8],m.v[9],m.v[10]};
 return std::abs(dot(x,x)-1)<.002f&&std::abs(dot(y,y)-1)<.002f&&std::abs(dot(z,z)-1)<.002f&&std::abs(dot(x,y))<.002f&&std::abs(dot(x,z))<.002f&&std::abs(dot(y,z))<.002f&&dot(x,cross(y,z))>.998f;
}
// CPU reference for diagnostics; runtime vertices are evaluated on the GPU.
inline Vec3 skinVertexPosition(const Vertex& v,const std::vector<Mat4>& matrices){
 Vec3 linear{};bool rigid=v.armSkinning>.5f;float total{};
 for(int k=0;k<4;++k)if(v.weights[k]>0){const auto& m=matrices.at(v.bones[k]);linear+=transformPoint(m,v.position)*v.weights[k];rigid&=rigidArmTransform(m);total+=v.weights[k];}
 if(!rigid||std::abs(total-1.f)>.001f)return linear;
 std::vector<volume_fit::Bone> fits(4);std::array<uint32_t,4> ids{0,1,2,3};
 for(int k=0;k<4;++k)if(v.weights[k]>0)fits[k]=volume_fit::prepare(matrices.at(v.bones[k]),{});
 return volume_fit::position(fits,v.position,ids,v.weights);
}
}
