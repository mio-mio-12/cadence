#pragma once
#include "SupportGripDiagnostic.h"
namespace diagnostic {
inline std::optional<scene::Mat4> nativePalm(const scene::CastScene& source,int side){
    using namespace scene;const auto a=imported::viewLayout(source).arms[side];if(a.wrist<0||a.fingers[1][0]<0||a.fingers[2][0]<0||a.fingers[4][0]<0)return {};
    const auto at=[&](int b){return transformPoint(source.skeleton.bones[b].restGlobal,{});};const auto p=at(a.wrist),x=normalize(at(a.fingers[2][0])-p),n=cross(at(a.fingers[1][0])-p,at(a.fingers[4][0])-p),y=normalize(cross(n,x)),z=normalize(cross(x,y));if(length(y)<.99f)return {};
    auto m=translation(p);m.v[0]=x.x;m.v[1]=x.y;m.v[2]=x.z;m.v[4]=y.x;m.v[5]=y.y;m.v[6]=y.z;m.v[8]=z.x;m.v[9]=z.y;m.v[10]=z.z;return m;
}
inline bool fullNativeGrasp(const scene::CastScene& source,const std::vector<scene::Mat4>& sourcePose,const scene::Mat4& frozenWeapon,
                            const scene::Skeleton& target,std::vector<scene::Mat4>& targetPose){
    using namespace scene;const auto view=imported::viewLayout(source);const auto body=imported::bodyOrCodLayout(target);const auto digits=imported::bodyFingerChains(target);auto result=targetPose;
    const auto determinant=[](const Mat4& m){return dot(cross(Vec3{m.v[0],m.v[1],m.v[2]},Vec3{m.v[4],m.v[5],m.v[6]}),Vec3{m.v[8],m.v[9],m.v[10]});};
    if(source.dualWield||determinant(frozenWeapon)<=0||sourcePose.size()!=source.skeleton.bones.size()||targetPose.size()!=target.bones.size())return false;
    // This experiment has verified only the Eldewrito authored +X gun root.
    const int gun=imported::world_grip::explicitGunAnchor(source.skeleton);if(source.importedViewGame!="eldewrito"||gun<0||view.arms[0].wrist<0||view.arms[1].wrist<0)return false;
    const auto& gm=sourcePose[gun];const auto forward=normalize(Vec3{gm.v[0],gm.v[1],gm.v[2]});
    if(dot(transformPoint(sourcePose[view.arms[1].wrist],{})-transformPoint(sourcePose[view.arms[0].wrist],{}),forward)>=-1.f)return false;
    const auto orient=[](const Mat4& m){Vec3 p,s;Quat q;decomposeAffine(m,p,q,s);return normalize(q);};const auto conj=[](Quat q){return Quat{-q.x,-q.y,-q.z,q.w};};
    const auto below=[&](int b,int root){for(std::size_t n=0;b>=0&&std::size_t(b)<target.bones.size()&&n<target.bones.size();++n){if(b==root)return true;b=target.bones[b].parent;}return false;};
    for(int side=0;side<2;++side){const auto sp=nativePalm(source,side),tp=imported::bodyPalmRestFrame(target,side);const auto& arm=view.arms[side];const int w=body.hand[side],middle=digits[side][2][0];if(!sp||!tp||w<0||middle<0)return false;
        const auto animated=frozenWeapon*sourcePose[arm.wrist]*inverseAffine(source.skeleton.bones[arm.wrist].restGlobal)**sp;
        auto palm=imported::world_grip::rigid(animated);if(!palm)return false;
        const auto sourceContact=transformPoint(frozenWeapon,(transformPoint(sourcePose[arm.wrist],{})+transformPoint(sourcePose[arm.fingers[2][0]],{}))*.5f);
        const auto targetContact=(transformPoint(target.bones[w].restGlobal,{})+transformPoint(target.bones[middle].restGlobal,{}))*.5f;
        const auto localContact=transformPoint(inverseAffine(*tp),targetContact);const auto offset=transformPoint(*palm,localContact)-transformPoint(*palm,{});const auto origin=sourceContact-offset;palm->v[12]=origin.x;palm->v[13]=origin.y;palm->v[14]=origin.z;
        const auto wrist=*palm*inverseAffine(*tp)*target.bones[w].restGlobal;const auto solved=placeSupport(target,result,transformPoint(wrist,{}),&wrist,side);
        std::cout<<"full_grasp_side="<<side<<" result="<<solved.reason<<" upper_error="<<solved.upperError<<" lower_error="<<solved.lowerError<<'\n';if(!solved.applied)return false;
        for(int d=0;d<5;++d){int sourceParent=arm.wrist,targetParent=w;for(int j=0;j<3;++j){const int sb=arm.fingers[d][j],tb=digits[side][d][j];if(sb<0||tb<0){std::cout<<"full_grasp_missing_digit="<<side<<','<<d<<'\n';return false;}
            const auto sr=inverseAffine(source.skeleton.bones[sourceParent].restGlobal)*source.skeleton.bones[sb].restGlobal;
            const auto sa=inverseAffine(sourcePose[sourceParent])*sourcePose[sb];const auto tr=inverseAffine(target.bones[targetParent].restGlobal)*target.bones[tb].restGlobal;
            const auto basis=orient(inverseAffine(target.bones[tb].restGlobal)**tp*inverseAffine(*sp)*source.skeleton.bones[sb].restGlobal);
            const auto delta=multiply(conj(orient(sr)),orient(sa)),transported=multiply(multiply(basis,delta),conj(basis));const auto desired=multiply(multiply(orient(result[targetParent]),orient(tr)),transported);
            const int parent=target.bones[tb].parent;const auto pg=parent>=0?result[parent]:Mat4::identity();const auto position=transformPoint(pg,target.bones[tb].restLocal.position);const auto wanted=trs(position,desired,target.bones[tb].restLocal.scale);const auto change=wanted*inverseAffine(result[tb]);for(std::size_t b=0;b<result.size();++b)if(below(int(b),tb))result[b]=change*result[b];sourceParent=sb;targetParent=tb;
        }}
    }
    for(std::size_t b=0;b<result.size();++b){for(float f:result[b].v)if(!std::isfinite(f))return false;const auto& m=result[b];if(dot(cross(Vec3{m.v[0],m.v[1],m.v[2]},Vec3{m.v[4],m.v[5],m.v[6]}),Vec3{m.v[8],m.v[9],m.v[10]})<=0)return false;if(!below(int(b),body.shoulder[0])&&!below(int(b),body.shoulder[1])&&result[b].v!=targetPose[b].v)return false;}
    for(int side=0;side<2;++side){const int shoulder=body.shoulder[side];if(length(transformPoint(result[shoulder],{})-transformPoint(targetPose[shoulder],{}))>.001f)return false;}
    targetPose=std::move(result);return true;
}
}
