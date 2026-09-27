#pragma once
#include "scene/ImportedBodyRetarget.h"

// Isolated experiment. No production sampling or renderer invokes this helper.
namespace diagnostic {
struct SupportResult { bool applied{}; const char* reason{"invalid anatomy"}; float upperError{},lowerError{},wristLinearError{},unrelatedError{}; };
inline SupportResult placeSupport(const scene::Skeleton& rig,std::vector<scene::Mat4>& pose,scene::Vec3 goal,const scene::Mat4* wristOrientation=nullptr,int side=0){
    using namespace scene;SupportResult result;const auto layout=imported::bodyOrCodLayout(rig);
    const int s=layout.shoulder[side],e=layout.elbow[side],w=layout.hand[side];
    if(s<0||e<0||w<0||pose.size()!=rig.bones.size())return result;
    const auto below=[&](int b,int root){for(std::size_t guard=0;b>=0&&std::size_t(b)<rig.bones.size()&&guard<rig.bones.size();++guard){if(b==root)return true;b=rig.bones[b].parent;}return false;};
    if(!below(e,s)||!below(w,e))return result;
    const auto at=[&](int b){return transformPoint(pose[b],{});};const auto a=at(s),b=at(e),c=at(w);
    const float upper=length(b-a),lower=length(c-b),reach=length(goal-a),epsilon=.001f;
    if(!std::isfinite(reach)||upper<epsilon||lower<epsilon){result.reason="degenerate limb";return result;}
    if(reach>=upper+lower-epsilon||reach<=std::abs(upper-lower)+epsilon){result.reason="unreachable";return result;}
    const auto axis=normalize(goal-a);auto pole=(b-a)-axis*dot(b-a,axis);
    if(length(pole)<epsilon){result.reason="singular bend plane";return result;}
    pole=normalize(pole);const float along=(upper*upper-lower*lower+reach*reach)/(2*reach);
    const float square=upper*upper-along*along;if(square<=0||!std::isfinite(square)){result.reason="invalid triangle";return result;}
    const auto nextElbow=a+axis*along+pole*std::sqrt(square);auto corrected=pose;
    const auto rotateTree=[&](int root,Vec3 pivot,Vec3 from,Vec3 to){
        from=normalize(from);to=normalize(to);const auto spin=cross(from,to);const float sine=length(spin),cosine=std::clamp(dot(from,to),-1.f,1.f);
        if(sine<1e-7f)return cosine>0;
        const auto change=translation(pivot)*rotation(fromAxisAngle(spin/sine,std::atan2(sine,cosine)))*translation(pivot*-1.f);
        for(std::size_t i=0;i<corrected.size();++i)if(below(int(i),root))corrected[i]=change*corrected[i];return true;
    };
    if(!rotateTree(s,a,b-a,nextElbow-a)){result.reason="opposite upper axis";return result;}
    const auto elbow=transformPoint(corrected[e],{}),wrist=transformPoint(corrected[w],{});
    if(!rotateTree(e,elbow,wrist-elbow,goal-elbow)){result.reason="opposite lower axis";return result;}
    auto desired=wristOrientation?*wristOrientation:pose[w];desired.v[12]=goal.x;desired.v[13]=goal.y;desired.v[14]=goal.z;
    const auto restore=desired*inverseAffine(corrected[w]);for(std::size_t i=0;i<corrected.size();++i)if(below(int(i),w))corrected[i]=restore*corrected[i];
    result.upperError=std::abs(length(transformPoint(corrected[e],{})-transformPoint(corrected[s],{}))-upper);
    result.lowerError=std::abs(length(transformPoint(corrected[w],{})-transformPoint(corrected[e],{}))-lower);
    for(int k=0;k<12;++k)result.wristLinearError=std::max(result.wristLinearError,std::abs(corrected[w].v[k]-desired.v[k]));
    for(std::size_t i=0;i<pose.size();++i){for(float f:corrected[i].v)if(!std::isfinite(f)){result.reason="nonfinite solve";return result;}if(!below(int(i),s))for(int k=0;k<16;++k)result.unrelatedError=std::max(result.unrelatedError,std::abs(corrected[i].v[k]-pose[i].v[k]));}
    if(result.upperError>.001f||result.lowerError>.001f||result.wristLinearError>1e-5f||result.unrelatedError!=0){result.reason="invariant failed";return result;}
    pose=std::move(corrected);result.applied=true;result.reason="applied";return result;
}
}
