#pragma once
#include "scene/ArmComponentSupport.h"
#include "scene/ProximalSupportFit.h"

namespace scene::imported {
// Shared live/replay integration candidate. Caller must explicitly opt in.
// Resolve the proximal roll carrier from native anatomy and skin usage, never
// from a weapon name or a donor-specific vertex list.
inline int nativeProximalRoll(const CastScene& native,const ViewArmLayout& arm){
    if(arm.shoulder<0||arm.elbow<0)return -1;
    const auto& bones=native.skeleton.bones;
    const auto origin=transformPoint(bones[arm.shoulder].restGlobal,{});
    const float span=length(transformPoint(bones[arm.elbow].restGlobal,{})-origin);
    if(!std::isfinite(span)||span<1e-4f)return -1;
    int best=-1;double greatest=0;bool tied=false;
    for(std::size_t i=0;i<bones.size();++i){
        const auto name=assets::imported::lower(bones[i].name);
        if(name.find("twist")==std::string::npos&&name.find("roll")==std::string::npos)continue;
        if(!proximal_support::below(native.skeleton,static_cast<int>(i),arm.shoulder)||
           proximal_support::below(native.skeleton,static_cast<int>(i),arm.elbow)||
           length(transformPoint(bones[i].restGlobal,{})-origin)>.1f*span)continue;
        double usage=0;for(const auto& mesh:native.meshes)if(mesh.skinned&&!mesh.viewmodelWeapon)
            for(const auto& v:mesh.vertices)for(int k=0;k<4;++k)if(v.bones[k]==i&&v.weights[k]>0)usage+=v.weights[k];
        if(usage>greatest+1e-6){greatest=usage;best=static_cast<int>(i);tied=false;}
        else if(usage>0&&std::abs(usage-greatest)<=1e-6)tied=true;
    }
    return tied?-1:best;
}
inline bool fitWithNativeSupport(CastScene& driver,CastScene skin,std::string& error,bool nativeArmEnvelope=false){
    const auto layout=viewLayout(driver);
    std::array<int,2> carriers{};
    for(int side=0;side<2;++side){
        carriers[side]=nativeProximalRoll(driver,layout.arms[side]);
        if(carriers[side]<0){error="Native proximal support candidate requires an unambiguous weighted shoulder roll on both arms";return false;}
    }
    const auto reference=driver;
    retainArmComponentSupport(skin);
    // Explicit shape experiment uses the existing anatomical radial-envelope
    // fitter; it never moves vertices along the arm or rescales palm/fingers.
    if(!fitForeignViewSkin(driver,skin,error,true,false,false,false,false,false,false,false,false,false,true,nativeArmEnvelope,true))return false;
    for(auto& mesh:driver.meshes)if(!mesh.viewmodelWeapon)for(auto& v:mesh.vertices)
        for(int k=0;k<4;++k)if(v.weights[k]>0)for(int side=0;side<2;++side)
            if(v.bones[k]==static_cast<unsigned>(layout.arms[side].shoulder))v.bones[k]=static_cast<unsigned>(carriers[side]);
    proximal_support::Report report;
    if(!proximal_support::applyDirectNative(driver,reference,report,error,false,true,true))return false;
    driver.warnings.push_back("Diagnostic shared native-support assembly: "+std::to_string(report.edited)+" proximal vertices supported");
    return true;
}
}
