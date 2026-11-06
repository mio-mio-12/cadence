#include "render/TrailGeometry.h"
#include "render/ViewmodelProjection.h"
#include <iostream>
#include <limits>
int main(){
    int failures=0;auto check=[&](bool ok){if(!ok)++failures;};
    for(float distance:{1.f,20.f,500.f,12000.f})for(float speed:{100.f,14000.f,50000.f}){
        const auto first=render::trails::interval(distance,0,speed,65);
        check(first.visible&&first.tail==0&&first.head<=distance);
        check(!render::trails::interval(distance,distance+66,speed,65).visible);
    }
    check(!render::trails::interval(100,0,std::numeric_limits<float>::quiet_NaN(),65).visible);
    check(render::trails::profile(0,1)==1&&render::trails::profile(1,1)==0);
    check(render::trails::profile(.8f,.9f)<render::trails::profile(.8f,.1f));
    std::vector<float> verts;
    render::trails::ribbon(verts,{0,0,0},{100,0,0},{0,1,0},{1,.5f,.2f},.8f,.5f,false,[](float u){return 1+u;});
    check(verts.size()==16*8*6*9);
    for(float x:verts)check(std::isfinite(x));
    for(size_t i=0;i<verts.size();i+=9){check(verts[i]>=0&&verts[i]<=100);check(verts[i+8]>=0&&verts[i+8]<=.8f);}
    const auto vp=scene::perspective(1.f,1.f,.1f,1000.f)*scene::lookAt({0,0,0},{1,0,0},{0,0,1});
    const auto muzzle=render::viewmodel_projection::worldOrigin({25,-3,2},{0,0,0},vp,70,1,true);
    check(scene::length(muzzle-scene::Vec3{25,3,2})<.0001f);
    std::cout<<"Trail geometry failures: "<<failures<<'\n';return failures?1:0;
}
