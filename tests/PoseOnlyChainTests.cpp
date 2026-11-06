#include "scene/PoseOnlyChain.h"
#include <iostream>
#include <limits>
int main(){
    using namespace scene;
    int failures=0;
    const auto check=[&](bool ok){if(!ok)++failures;};
    const std::vector<Mat4> rest{translation({0,0,0}),translation({30,0,0}),translation({55,0,0}),translation({62,0,0})};
    std::vector<Mat4> output;
    for(float units:{1.f,2.54f})for(int frame=0;frame<=120;++frame){
        const float t=float(frame)/120;
        std::vector<Quat> q{fromAxisAngle({0,0,1},t*3),fromAxisAngle({0,1,0},t*-2),fromAxisAngle({1,0,0},t*4),Quat{}};
        for(std::size_t anchor=0;anchor<rest.size();++anchor){
            check(pose_only::chain(rest,q,anchor,{4,8,12},units,output));
            check(length(transformPoint(output[anchor],{})-Vec3{4,8,12})<1e-4f);
            for(std::size_t i=0;i+1<rest.size();++i){
                const float wanted=length(transformPoint(rest[i+1],{})-transformPoint(rest[i],{}))*units;
                check(std::abs(length(transformPoint(output[i+1],{})-transformPoint(output[i],{}))-wanted)<1e-4f);
            }
            // Bone transforms do not thin/inflate attached rigid geometry.
            check(std::abs(length(transformPoint(output[1],{0,3,0})-transformPoint(output[1],{}))-3)<1e-4f);
        }
    }
    std::vector<Quat> q(4);const auto saved=output;
    check(!pose_only::chain(rest,q,4,{},1,output));
    check(!pose_only::chain(rest,q,0,{},0,output));
    auto bad=rest;bad[1].v[0]=.5f;check(!pose_only::chain(bad,q,0,{},1,output));
    q[0]={0,0,0,0};check(!pose_only::chain(rest,q,0,{},1,output));
    check(output.size()==saved.size());for(std::size_t i=0;i<output.size();++i)check(output[i].v==saved[i].v);
    std::cout<<"Pose-only donor length/anchor/rigidity tests: "<<failures<<" failures\n";
    return failures?1:0;
}
