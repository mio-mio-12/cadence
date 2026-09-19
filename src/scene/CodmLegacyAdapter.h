#pragma once
#include "scene/CodmNative.h"
#include "scene/VolumePreservingFit.h"

namespace scene::codm {
// Fit a copy of the target mesh/bind rig to a native hand reference. Playback
// stays on the native driver and is converted after all native-space layers.
// No Euler transfer, animation baking, file changes or weapon wrist attachment.
inline Mat4 anatomicalFrame(Vec3 origin,Vec3 next,Vec3 palmNormal){
    const auto x=normalize(next-origin);auto y=normalize(cross(palmNormal,x));
    if(length(x)<.5f||length(y)<.5f)throw std::runtime_error("Degenerate CODM anatomical frame");
    const auto z=normalize(cross(x,y));Mat4 m=Mat4::identity();
    m.v[0]=x.x;m.v[1]=x.y;m.v[2]=x.z;m.v[4]=y.x;m.v[5]=y.y;m.v[6]=y.z;m.v[8]=z.x;m.v[9]=z.y;m.v[10]=z.z;m.v[12]=origin.x;m.v[13]=origin.y;m.v[14]=origin.z;return m;
}
inline bool fitLegacyHands(CastScene& native,const CastScene& reference,const cast::Document& legacyDocument,std::string& error){
    if((!native.codmNativeCentimetres&&!native.pointBlankNativeCentimetres)||reference.skeleton.bones.empty()){error="Normalized native reference required";return false;}
    auto legacy=buildScene(legacyDocument);if(legacy.meshes.empty()){error="Legacy hands have no mesh";return false;}
    const auto& src=reference.skeleton;const auto& dst=legacy.skeleton;
    auto adapter=std::make_shared<CodmRigAdapter>();adapter->firstBone=native.skeleton.bones.size();adapter->identity=std::string(native.pointBlankNativeCentimetres?"Point Blank anatomical fit v2 / ":"CODM anatomical fit v1 / ")+std::filesystem::path(legacyDocument.sourceName()).filename().string();
    // Identity tracks calibration inputs, not filenames alone. This is an
    // in-memory diagnostic fingerprint, not a cryptographic asset digest.
    uint64_t fingerprint=14695981039346656037ull;
    const auto hash=[&](const void* data,size_t count){const auto* bytes=static_cast<const unsigned char*>(data);while(count--){fingerprint^=*bytes++;fingerprint*=1099511628211ull;}};
    for(const auto* skeleton:{&src,&dst})for(const auto& b:skeleton->bones){hash(b.name.data(),b.name.size());hash(&b.parent,sizeof(b.parent));hash(b.restGlobal.v.data(),sizeof(float)*16);}
    for(const auto& m:legacy.meshes){for(const auto& v:m.vertices){hash(&v.position,sizeof(v.position));hash(v.bones.data(),sizeof(v.bones));hash(v.weights.data(),sizeof(v.weights));}hash(m.indices.data(),m.indices.size()*sizeof(uint32_t));hash(m.modelTransform.v.data(),sizeof(float)*16);}
    adapter->identity+=" / bind+skin="+std::to_string(fingerprint);
    std::vector<Mat4> deformation(dst.bones.size(),Mat4::identity()),fitted(dst.bones.size(),Mat4::identity());
    std::vector<int> source(dst.bones.size(),-1);std::vector<bool> mapped(dst.bones.size());
    const bool oneBased=!dst.boneByName.contains("j_pinky_le_0")&&!dst.boneByName.contains("j_pinky_ri_0")&&dst.boneByName.contains("j_metaindex_le_1")&&dst.boneByName.contains("j_metaindex_ri_1");
    const auto index=[&](const Skeleton& s,std::string name)->size_t{
        if(&s==&dst&&oneBased&&(name.starts_with("j_index_")||name.starts_with("j_mid_")||name.starts_with("j_ring_")||name.starts_with("j_pinky_")||name.starts_with("j_thumb_"))&&name.back()>='0'&&name.back()<='2')++name.back();
        auto it=s.boneByName.find(name);if(it!=s.boneByName.end())return it->second;throw std::runtime_error("Missing anatomical joint: "+name);
    };
    const auto pos=[](const Skeleton&s,size_t i){return transformPoint(s.bones[i].restGlobal,{});};
    const auto descendant=[](const Skeleton& s,size_t child,size_t parent){int p=s.bones[child].parent;size_t remaining=s.bones.size();while(p>=0&&remaining--){if(static_cast<size_t>(p)==parent)return true;if(static_cast<size_t>(p)>=s.bones.size())return false;p=s.bones[p].parent;}return false;};
    try{
        if(oneBased)adapter->helperRules.push_back("Verified metacarpal layout: legacy finger 1/2/3 -> native 1/2/3; 4 is a distal helper.");
        for(const auto side:{std::pair{"le","Left"},std::pair{"ri","Right"}}){
            const std::string dside=side.first,sside=side.second;
            const auto dw=index(dst,"j_wrist_"+dside),sw=index(src,"b_"+sside+"Hand");
            const auto di=index(dst,"j_index_"+dside+"_0"),si=index(src,"b_"+sside+"Index1");
            const auto dp=index(dst,"j_pinky_"+dside+"_0"),sp=index(src,"b_"+sside+"Pinky1");
            const auto dn=normalize(cross(pos(dst,di)-pos(dst,dw),pos(dst,dp)-pos(dst,dw))),sn=normalize(cross(pos(src,si)-pos(src,sw),pos(src,sp)-pos(src,sw)));
            const float width=length(pos(src,si)-pos(src,sp))/length(pos(dst,di)-pos(dst,dp));
            if(!std::isfinite(width)||width<.3f||width>3.f)throw std::runtime_error("Implausible CODM palm fit scale");
            const auto map=[&](std::string dnme,std::string snme,std::string dnext,std::string snext,Vec3 dnormal=Vec3{},Vec3 snormal=Vec3{}){
                const auto d=index(dst,dnme),s=index(src,snme),dc=index(dst,dnext),sc=index(src,snext);const auto d0=pos(dst,d),s0=pos(src,s);
                if(!descendant(dst,dc,d)||!descendant(src,sc,s))throw std::runtime_error("Anatomical chain relationship mismatch: "+dnme);
                const float axial=length(pos(src,sc)-s0)/length(pos(dst,dc)-d0);
                if(!std::isfinite(axial)||axial<.25f||axial>4.f)throw std::runtime_error("Implausible CODM segment fit: "+dnme);
                deformation[d]=anatomicalFrame(s0,pos(src,sc),length(snormal)>.5f?snormal:sn)*scale({axial,width,width})*inverseAffine(anatomicalFrame(d0,pos(dst,dc),length(dnormal)>.5f?dnormal:dn));source[d]=static_cast<int>(s);mapped[d]=true;
            };
            map("j_shoulder_"+dside,"b_"+sside+"Arm","j_elbow_"+dside,"b_"+sside+"ForeArm");
            map("j_elbow_"+dside,"b_"+sside+"ForeArm","j_wrist_"+dside,"b_"+sside+"Hand");
            map("j_wrist_"+dside,"b_"+sside+"Hand","j_mid_"+dside+"_0","b_"+sside+"Middle1");
            for(const auto finger:{std::pair{"thumb","Thumb"},std::pair{"index","Index"},std::pair{"mid","Middle"},std::pair{"ring","Ring"},std::pair{"pinky","Pinky"}}){
                Vec3 dnormal=dn,snormal=sn,previousD{},previousS{};
                const auto transport=[](Vec3 normal,Vec3 from,Vec3 to){from=normalize(from);to=normalize(to);const float c=std::clamp(dot(from,to),-1.f,1.f);Quat q;if(c<-.9999f)q=fromAxisAngle(normal,kPi);else{auto axis=cross(from,to);q=normalize(Quat{axis.x,axis.y,axis.z,1+c});}auto r=rotation(q);return normalize(volume_fit::vector(r,normal));};
                for(int joint=0;joint<2;++joint){
                    const auto dname="j_"+std::string(finger.first)+"_"+dside+"_"+std::to_string(joint),sname="b_"+sside+finger.second+std::to_string(joint+1),dnext="j_"+std::string(finger.first)+"_"+dside+"_"+std::to_string(joint+1),snext="b_"+sside+finger.second+std::to_string(joint+2);
                    const auto da=normalize(pos(dst,index(dst,dnext))-pos(dst,index(dst,dname))),sa=normalize(pos(src,index(src,snext))-pos(src,index(src,sname)));
                    if(native.pointBlankNativeCentimetres){if(joint==0){dnormal=normalize(dn-da*dot(dn,da));snormal=normalize(sn-sa*dot(sn,sa));}else{dnormal=transport(dnormal,previousD,da);snormal=transport(snormal,previousS,sa);}}
                    map(dname,sname,dnext,snext,dnormal,snormal);previousD=da;previousS=sa;
                }
                const auto d=index(dst,"j_"+std::string(finger.first)+"_"+dside+"_2"),s=index(src,"b_"+sside+finger.second+"3");
                // Terminal phalanx: retain its preceding anatomical-frame
                // correction, reanchored at the actual distal joint.
                const auto parent=index(dst,"j_"+std::string(finger.first)+"_"+dside+"_1");deformation[d]=deformation[parent];const auto p=pos(src,s)-transformPoint(deformation[d],pos(dst,d));deformation[d].v[12]+=p.x;deformation[d].v[13]+=p.y;deformation[d].v[14]+=p.z;source[d]=static_cast<int>(s);mapped[d]=true;
                if(native.pointBlankNativeCentimetres){
                    // Distal bones have no child landmark. Use their actual
                    // skinned phalanx centers, not an assumed terminal axis.
                    const auto sp=index(src,"b_"+sside+finger.second+"2");
                    const auto direction=[](const Mat4&m,Vec3 v){return Vec3{m.v[0]*v.x+m.v[4]*v.y+m.v[8]*v.z,m.v[1]*v.x+m.v[5]*v.y+m.v[9]*v.z,m.v[2]*v.x+m.v[6]*v.y+m.v[10]*v.z};};
                    auto da=direction(dst.bones[d].restGlobal,direction(inverseAffine(dst.bones[parent].restGlobal),pos(dst,d)-pos(dst,parent)));
                    auto sa=direction(src.bones[s].restGlobal,direction(inverseAffine(src.bones[sp].restGlobal),pos(src,s)-pos(src,sp)));
                    const auto skinAxis=[](const CastScene& model,size_t bone,Vec3 origin,Vec3 fallback){Vec3 center{};float total=0;for(auto&m:model.meshes)for(auto&v:m.vertices){float w=0;for(int k=0;k<4;++k)if(v.bones[k]==bone)w+=v.weights[k];if(w<.75f)continue;center+=transformPoint(m.modelTransform,v.position)*w;total+=w;}if(total>0){auto axis=center/total-origin;if(length(axis)>.2f&&length(axis)<8.f)return axis;}return fallback;};
                    da=skinAxis(legacy,d,pos(dst,d),da);sa=skinAxis(reference,s,pos(src,s),sa);
                    const float axial=length(sa)/length(da);
                    if(!std::isfinite(axial)||axial<.25f||axial>4.f)throw std::runtime_error("Implausible Point Blank distal finger fit");
                    dnormal=transport(dnormal,previousD,da);snormal=transport(snormal,previousS,sa);
                    deformation[d]=anatomicalFrame(pos(src,s),pos(src,s)+sa,snormal)*scale({axial,width,width})*inverseAffine(anatomicalFrame(pos(dst,d),pos(dst,d)+da,dnormal));
                }
            }
        }
        const auto fallback=index(src,"b_Spine");
        for(size_t d=0;d<dst.bones.size();++d){
            if(!mapped[d]){int p=dst.bones[d].parent;while(p>=0&&!mapped[p])p=dst.bones[p].parent;if(p>=0){deformation[d]=deformation[p];source[d]=source[p];adapter->helperRules.push_back(dst.bones[d].name+" follows "+dst.bones[p].name+" in fitted bind space");}else source[d]=static_cast<int>(fallback);}
            fitted[d]=deformation[d]*dst.bones[d].restGlobal;
            for(float v:fitted[d].v)if(!std::isfinite(v))throw std::runtime_error("Non-finite fitted legacy bind");
            const auto s=static_cast<size_t>(source[d]);const auto driver=index(native.skeleton,src.bones[s].name);CodmRigBinding binding{driver,inverseAffine(src.bones[s].restGlobal)*fitted[d]};
            if(!mapped[d]&&(src.bones[s].name=="b_LeftForeArm"||src.bones[s].name=="b_RightForeArm")){
                const auto side=src.bones[s].name=="b_LeftForeArm"?"Left":"Right";const auto roll=index(src,"b_"+std::string(side)+"ForeArmRoll"),wrist=index(src,"b_"+std::string(side)+"Hand");const auto arm=pos(src,wrist)-pos(src,s);binding.rollWeight=std::clamp(dot(transformPoint(fitted[d],{})-pos(src,s),arm)/dot(arm,arm),0.f,1.f);binding.rollSource=index(native.skeleton,src.bones[roll].name);binding.rollOffset=inverseAffine(src.bones[roll].restGlobal)*fitted[d];
                adapter->helperRules.push_back(dst.bones[d].name+": native forearm roll distributed by bind-space distance ("+std::to_string(binding.rollWeight)+")");
            }
            adapter->bindings.push_back(binding);
        }
        // Some IW_SP hand exports also contain full-body meshes. Keep only
        // arm-weighted triangles in this first-person copy; never fit a body.
        std::vector<bool> armBone(dst.bones.size());for(size_t d=0;d<dst.bones.size();++d){int p=static_cast<int>(d);while(p>=0){if(dst.bones[p].name=="j_shoulder_le"||dst.bones[p].name=="j_shoulder_ri"){armBone[d]=true;break;}p=dst.bones[p].parent;}}
        std::vector<volume_fit::Bone> volumeFits;
        if(native.pointBlankNativeCentimetres){volumeFits.reserve(deformation.size());for(size_t d=0;d<deformation.size();++d)volumeFits.push_back(volume_fit::prepare(deformation[d],pos(dst,d)));}
        size_t removedTriangles=0;
        for(auto& mesh:legacy.meshes){std::vector<bool> armVertex(mesh.vertices.size());for(size_t v=0;v<mesh.vertices.size();++v){float w=0;for(int k=0;k<4;++k)if(mesh.vertices[v].bones[k]<armBone.size()&&armBone[mesh.vertices[v].bones[k]])w+=mesh.vertices[v].weights[k];armVertex[v]=w>.999f;}std::vector<uint32_t> indices;for(size_t i=0;i+2<mesh.indices.size();i+=3){if(mesh.indices[i]>=armVertex.size()||mesh.indices[i+1]>=armVertex.size()||mesh.indices[i+2]>=armVertex.size())throw std::runtime_error("Invalid legacy triangle index");if(armVertex[mesh.indices[i]]&&armVertex[mesh.indices[i+1]]&&armVertex[mesh.indices[i+2]])indices.insert(indices.end(),mesh.indices.begin()+i,mesh.indices.begin()+i+3);else ++removedTriangles;}mesh.indices=std::move(indices);
            for(auto& v:mesh.vertices){const auto p=transformPoint(mesh.modelTransform,v.position);Vec3 fittedPosition{},normal{};float weight=0;for(int k=0;k<4;++k)if(v.weights[k]>0){const auto b=v.bones[k];if(b>=deformation.size())throw std::runtime_error("Invalid legacy mesh weight");fittedPosition+=transformPoint(deformation[b],p)*v.weights[k];const auto nmat=inverseAffine(deformation[b]*mesh.modelTransform);normal+=Vec3{nmat.v[0]*v.normal.x+nmat.v[1]*v.normal.y+nmat.v[2]*v.normal.z,nmat.v[4]*v.normal.x+nmat.v[5]*v.normal.y+nmat.v[6]*v.normal.z,nmat.v[8]*v.normal.x+nmat.v[9]*v.normal.y+nmat.v[10]*v.normal.z}*v.weights[k];weight+=v.weights[k];}
                if(weight<.99f||weight>1.01f)throw std::runtime_error("Non-normalized legacy skin weights");if(volumeFits.empty())v.position=fittedPosition;else{Mat4 differential;v.position=volume_fit::position(volumeFits,p,v.bones,v.weights,&differential);normal=volume_fit::vector(volume_fit::transpose3(inverseAffine(differential*mesh.modelTransform)),v.normal);}v.normal=normalize(normal);for(auto& b:v.bones)b+=static_cast<uint32_t>(adapter->firstBone);
            }mesh.modelTransform=Mat4::identity();mesh.viewmodelWeapon=false;}
        legacy.meshes.erase(std::remove_if(legacy.meshes.begin(),legacy.meshes.end(),[](const auto& m){return m.indices.empty();}),legacy.meshes.end());if(legacy.meshes.empty())throw std::runtime_error("No arm-weighted legacy triangles");
        if(removedTriangles)adapter->helperRules.push_back("Excluded "+std::to_string(removedTriangles)+" non-arm triangles from first-person copy.");
        for(size_t d=0;d<legacy.skeleton.bones.size();++d){auto b=legacy.skeleton.bones[d];b.name="codm_legacy|"+b.name;if(b.parent>=0)b.parent+=static_cast<int32_t>(adapter->firstBone);b.restGlobal=fitted[d];b.inverseBind=inverseAffine(fitted[d]);const auto local=dst.bones[d].parent>=0?inverseAffine(fitted[dst.bones[d].parent])*fitted[d]:fitted[d];decomposeAffine(local,b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);native.skeleton.boneByName[b.name]=native.skeleton.bones.size();native.skeleton.bones.push_back(b);}
        native.meshes.erase(std::remove_if(native.meshes.begin(),native.meshes.end(),[](const auto& m){return !m.viewmodelWeapon;}),native.meshes.end());for(auto& m:legacy.meshes)native.meshes.push_back(std::move(m));native.codmRigAdapter=std::move(adapter);
        native.warnings.push_back("CODM legacy anatomical adapter candidate; visual validation required. Native weapon and camera tracks unchanged.");return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
}
