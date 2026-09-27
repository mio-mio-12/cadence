#include "render/NativeViewhandSkinning.h"
#include <iostream>
int main(){
    int failures{};const auto check=[&](bool pass){if(!pass)++failures;};
    scene::CastScene s;s.importedViewGame="eldewrito";s.skeleton.bones.resize(4);
    const char* names[]={"l_forearm","l_hand","finger","gun"};
    for(int i=0;i<4;++i){s.skeleton.bones[i].name=names[i];s.skeleton.boneByName[names[i]]=i;}
    s.skeleton.bones[1].parent=0;s.skeleton.bones[1].restLocal.position={30,0,0};
    const auto q=scene::fromEulerRadians({2.1f,.2f,0});
    std::vector<scene::Mat4> pose{scene::Mat4::identity(),scene::trs({30,0,0},q,{1,1,1}),scene::Mat4::identity(),scene::Mat4::identity()};
    const auto original=pose;auto skin=pose;render::nativeViewhandSkinning(s,pose,skin);
    check(skin[0].v!=original[0].v);for(int i=1;i<4;++i)check(skin[i].v==original[i].v);for(int i=0;i<4;++i)check(pose[i].v==original[i].v);
    const auto corrected=skin;render::nativeViewhandSkinning(s,pose,skin);check(skin[0].v==corrected[0].v); // repeat passes cannot accumulate roll
    s.viewHandsDriverGame="eldewrito";skin=pose;render::nativeViewhandSkinning(s,pose,skin);check(skin[0].v==pose[0].v); // foreign skins unchanged
    s.viewHandsDriverGame.clear();s.importedViewGame="bo2";render::nativeViewhandSkinning(s,pose,skin);check(skin[0].v==pose[0].v);
    s.importedViewGame="eldewrito";pose[1]=scene::trs({30,0,0},{0,1,0,0},{1,1,1});skin=pose;render::nativeViewhandSkinning(s,pose,skin);check(skin[0].v==pose[0].v); // singular swing
    for(int frame=0;frame<=720;++frame){pose[1]=scene::trs({30,0,0},scene::fromEulerRadians({frame*scene::kPi/180,.1f,0}),{1,1,1});skin=pose;render::nativeViewhandSkinning(s,pose,skin);for(const auto& m:skin)for(auto f:m.v)check(std::isfinite(f));check(skin[1].v==pose[1].v);}
    return failures?1:0;
}
