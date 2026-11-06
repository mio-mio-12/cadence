#pragma once
#include "scene/CastScene.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace scene::imported {
// Review-only hypothesis: spread a rigid forearm/wrist seam over a short
// distal forearm band. Never changes bind geometry, palm or authored blends.
inline size_t wristTransitionDiagnostic(std::vector<Mesh>& meshes,const Skeleton& rig,
                                       int elbow,int wrist,bool enabled=false,bool refine=false){
    if(!enabled||elbow<0||wrist<0||elbow==wrist||size_t(elbow)>=rig.bones.size()||size_t(wrist)>=rig.bones.size())return 0;
    const auto origin=transformPoint(rig.bones[elbow].restGlobal,{});
    const auto axis=transformPoint(rig.bones[wrist].restGlobal,{})-origin;
    const auto span=length(axis);if(!std::isfinite(span)||span<1e-4f)return 0;
    const auto direction=axis/span;size_t changed=0;
    const auto owner=[&](const Vertex& v){float e=0,w=0,total=0;
        for(int k=0;k<4;++k){const float q=v.weights[k];if(!std::isfinite(q)||q<0)return -1;total+=q;
            if(v.bones[k]==uint32_t(elbow))e+=q;else if(v.bones[k]==uint32_t(wrist))w+=q;else if(q>0)return -1;}
        if(std::abs(total-1)>1e-5f)return -1;
        return e>1-1e-5f?0:w>1-1e-5f?1:-1;};
    for(auto& mesh:meshes){if(mesh.viewmodelWeapon||!mesh.skinned)continue;
        std::vector<float> stations;std::vector<int> owners;bool valid=true;
        for(const auto& v:mesh.vertices){const auto p=transformPoint(mesh.modelTransform,v.position);
            const float t=dot(p-origin,direction);valid&=std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(t);
            stations.push_back(t);owners.push_back(owner(v));}
        float seam=std::numeric_limits<float>::max(),end=-seam;size_t crossings=0;
        for(size_t i=0;i+2<mesh.indices.size();i+=3)for(int k=0;k<3;++k){auto a=mesh.indices[i+k],b=mesh.indices[i+(k+1)%3];
            if(a>=owners.size()||b>=owners.size()){valid=false;continue;}
            if(owners[a]==1&&owners[b]==0)std::swap(a,b);
            if(owners[a]==0&&owners[b]==1){seam=std::min(seam,stations[a]);end=std::max(end,stations[a]);++crossings;}}
        if(!valid||crossings<3||seam<span*.55f||end>span*1.1f||end-seam>span*.2f)continue;
        const float start=seam-span*.2f;
        if(refine){
            auto refined=mesh;auto ts=stations;auto os=owners;bool safe=true;
            const auto limit=mesh.vertices.size()+4096;
            for(int plane=0;plane<=8&&safe;++plane){
                const float cut=start+(seam-start)*plane/8.f;
                std::map<std::pair<uint32_t,uint32_t>,uint32_t> intersections;
                std::vector<uint32_t> indices;
                const auto intersect=[&](uint32_t a,uint32_t b){
                    if(a>b)std::swap(a,b);const auto key=std::make_pair(a,b);
                    if(auto it=intersections.find(key);it!=intersections.end())return it->second;
                    const float t=(cut-ts[a])/(ts[b]-ts[a]);const auto& x=refined.vertices[a];const auto& y=refined.vertices[b];
                    Vertex v=x;v.position=x.position*(1-t)+y.position*t;v.normal=normalize(x.normal*(1-t)+y.normal*t);
                    v.uv={x.uv.x*(1-t)+y.uv.x*t,x.uv.y*(1-t)+y.uv.y*t};
                    v.color={x.color.x*(1-t)+y.color.x*t,x.color.y*(1-t)+y.color.y*t,x.color.z*(1-t)+y.color.z*t,x.color.w*(1-t)+y.color.w*t};
                    v.armSkinning=std::min(x.armSkinning,y.armSkinning);
                    const auto id=uint32_t(refined.vertices.size());refined.vertices.push_back(v);ts.push_back(cut);os.push_back(0);intersections[key]=id;
                    if(refined.vertices.size()>limit)safe=false;return id;
                };
                for(size_t i=0;i+2<refined.indices.size()&&safe;i+=3){
                    const std::array<uint32_t,3> tri{refined.indices[i],refined.indices[i+1],refined.indices[i+2]};
                    float lo=std::min({ts[tri[0]],ts[tri[1]],ts[tri[2]]}),hi=std::max({ts[tri[0]],ts[tri[1]],ts[tri[2]]});
                    if(lo>=cut||hi<=cut){indices.insert(indices.end(),tri.begin(),tri.end());continue;}
                    if(os[tri[0]]!=0||os[tri[1]]!=0||os[tri[2]]!=0){
                        // Reject rather than create a T-junction against a
                        // triangle carrying an authored blend or other joint.
                        for(int e=0;e<3;++e){const auto a=tri[e],b=tri[(e+1)%3];if(os[a]==0&&os[b]==0&&std::min(ts[a],ts[b])<cut&&std::max(ts[a],ts[b])>cut)safe=false;}
                        indices.insert(indices.end(),tri.begin(),tri.end());continue;
                    }
                    for(bool lower:{true,false}){std::vector<uint32_t> polygon;
                        for(int e=0;e<3;++e){const auto a=tri[e],b=tri[(e+1)%3];const bool ia=lower?ts[a]<=cut:ts[a]>=cut,ib=lower?ts[b]<=cut:ts[b]>=cut;
                            if(ia)polygon.push_back(a);if(ia!=ib&&ts[a]!=cut&&ts[b]!=cut)polygon.push_back(intersect(a,b));}
                        for(size_t p=1;p+1<polygon.size();++p)indices.insert(indices.end(),{polygon[0],polygon[p],polygon[p+1]});
                    }
                }
                refined.indices=std::move(indices);
            }
            if(!safe)continue;
            mesh=std::move(refined);stations=std::move(ts);owners=std::move(os);
        }
        for(size_t i=0;i<mesh.vertices.size();++i)if(owners[i]==0&&stations[i]>start){
            const float t=std::clamp((stations[i]-start)/(seam-start),0.f,1.f),w=t*t*(3-2*t);
            auto& v=mesh.vertices[i];v.bones={uint32_t(elbow),uint32_t(wrist),0,0};v.weights={1-w,w,0,0};++changed;
        }
    }
    return changed;
}
}
