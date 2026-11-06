#pragma once
#include "scene/CastScene.h"
#include <map>
#include <optional>
#include <cctype>

namespace scene::imported {
// Measured native extent, not a camera-dependent extrusion length. Reject
// absent/ambiguous support rather than inventing a torso-sized continuation.
inline std::optional<float> nativeProximalExtent(const CastScene& native,int shoulder,int elbow){
    const auto& rig=native.skeleton;
    if(shoulder<0||elbow<0||std::size_t(shoulder)>=rig.bones.size()||std::size_t(elbow)>=rig.bones.size())return {};
    const auto origin=transformPoint(rig.bones[shoulder].restGlobal,{}),axis=transformPoint(rig.bones[elbow].restGlobal,{})-origin;
    const float length2=dot(axis,axis);if(!std::isfinite(length2)||length2<1e-8f)return {};
    std::vector<std::size_t> canonical(rig.bones.size());for(std::size_t i=0;i<canonical.size();++i)canonical[i]=i;
    for(auto [f,b]:native.nativePoseFollowers)if(f<canonical.size()&&b<canonical.size())canonical[f]=b;
    std::vector<Vec3> support;float extent=0;
    for(const auto& mesh:native.meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices){
        Vec3 p{};float sum=0,owned=0;
        for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<canonical.size()){
            const auto b=canonical[v.bones[k]];const float w=v.weights[k];sum+=w;
            p+=transformPoint(rig.bones[b].restGlobal*rig.bones[v.bones[k]].inverseBind,transformPoint(mesh.modelTransform,v.position))*w;
            if(b==std::size_t(shoulder))owned+=w;
            else if(rig.bones[b].parent==shoulder){
                std::string name=rig.bones[b].name;for(auto& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if(name.find("twist")!=std::string::npos||name.find("roll")!=std::string::npos)owned+=w;
            }
        }
        if(sum<.99f||sum>1.01f||owned<.5f)continue;
        const auto delta=p-origin;const float t=dot(delta,axis)/length2;
        if(!std::isfinite(t)||t>=0||dot(delta-axis*t,delta-axis*t)>length2*.25f)continue;
        if(t<-.5f)return {}; // Unbounded shoulder/torso ownership is not a sleeve endpoint.
        bool duplicate=false;for(auto q:support)if(length(q-p)<1e-4f){duplicate=true;break;}
        if(!duplicate)support.push_back(p);extent=std::max(extent,-t);
    }
    if(support.size()<3||extent<1e-4f)return {};
    return extent;
}
// Diagnostic only. Keep bind geometry and distal influences untouched.
inline std::size_t nativeProximalDiagnostic(const CastScene& native,std::vector<Mesh>& fitted,int shoulder,int elbow,bool enabled=false,bool coherent=false,bool transition=false,bool localShoulder=false){
    const auto& rig=native.skeleton;
    if(!enabled||shoulder<0||elbow<0||std::size_t(shoulder)>=rig.bones.size()||std::size_t(elbow)>=rig.bones.size())return 0;
    const auto origin=transformPoint(rig.bones[shoulder].restGlobal,{}),axis=transformPoint(rig.bones[elbow].restGlobal,{})-origin;
    const float length2=dot(axis,axis);if(!std::isfinite(length2)||length2<1e-8f)return 0;
    std::vector<std::size_t> canonical(rig.bones.size());for(std::size_t i=0;i<canonical.size();++i)canonical[i]=i;
    for(auto [f,b]:native.nativePoseFollowers)if(f<canonical.size()&&b<canonical.size())canonical[f]=b;
    struct Sample{Vec3 point;std::map<uint32_t,float> weights;};std::vector<Sample> samples;
    for(const auto& mesh:native.meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices){
        Sample s{};float ownership=0,sum=0;
        for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<canonical.size()){
            const auto b=canonical[v.bones[k]];const float w=v.weights[k];s.weights[static_cast<uint32_t>(b)]+=w;sum+=w;
            s.point+=transformPoint(rig.bones[b].restGlobal*rig.bones[v.bones[k]].inverseBind,transformPoint(mesh.modelTransform,v.position))*w;
            int ancestor=static_cast<int>(b);
            for(std::size_t n=0;ancestor>=0&&std::size_t(ancestor)<rig.bones.size()&&n<rig.bones.size();++n){
                if(ancestor==elbow)break;if(ancestor==shoulder){ownership+=w;break;}ancestor=rig.bones[ancestor].parent;
            }
        }
        const float t=dot(s.point-origin,axis)/length2;
        if(sum>.99f&&sum<1.01f&&ownership>=.5f&&t>=-.1f&&t<=.45f)samples.push_back(std::move(s));
    }
    std::size_t changed=0;
    std::vector<std::pair<Vertex*,Vertex>> edits;
    for(auto& mesh:fitted)if(!mesh.viewmodelWeapon)for(auto& v:mesh.vertices){
        float shoulderWeight=0;for(int k=0;k<4;++k)if(v.bones[k]==std::size_t(shoulder))shoulderWeight+=v.weights[k];
        const float t=dot(v.position-origin,axis)/length2;if(shoulderWeight<.99f||t<-.1f||t>=.4f)continue;
        std::vector<std::pair<float,std::size_t>> nearest;
        for(std::size_t i=0;i<samples.size();++i){const auto delta=samples[i].point-v.position;const float d=dot(delta,delta);if(d<=length2*.0625f)nearest.emplace_back(d,i);}
        if(nearest.empty())continue;std::sort(nearest.begin(),nearest.end());if(nearest.size()>4)nearest.resize(4);
        std::map<uint32_t,float> weights;float norm=0;for(auto [distance,i]:nearest){const float w=1.f/std::max(distance,1e-6f);norm+=w;for(auto [b,value]:samples[i].weights)weights[b]+=value*w;}
        const float x=std::clamp((t-.1f)/.3f,0.f,1.f),blend=1-x*x*(3-2*x);
        for(auto& [b,w]:weights)w=w/norm*blend;weights[static_cast<uint32_t>(shoulder)]+=1-blend;
        std::vector<std::pair<uint32_t,float>> sorted(weights.begin(),weights.end());std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.second>b.second;});
        if(coherent)edits.emplace_back(&v,v);
        v.bones={};v.weights={};norm=0;for(std::size_t k=0;k<std::min<std::size_t>(4,sorted.size());++k){v.bones[k]=sorted[k].first;v.weights[k]=sorted[k].second;norm+=v.weights[k];}for(auto& w:v.weights)w/=norm;++changed;
    }
    if(coherent&&!edits.empty()){
        // Narrow cross-section experiment only; never flatten a whole upper arm.
        float low=1e9f,high=-1e9f;std::vector<Vec3> unique;std::map<uint32_t,float> mean;
        for(const auto& [v,before]:edits){
            const float t=dot(v->position-origin,axis)/length2;low=std::min(low,t);high=std::max(high,t);
            bool duplicate=false;for(auto p:unique)if(length(p-v->position)<1e-4f){duplicate=true;break;}
            if(duplicate)continue;unique.push_back(v->position);
            for(int k=0;k<4;++k)if(v->weights[k]>0)mean[v->bones[k]]+=v->weights[k];
        }
        if(unique.size()<3||high-low>.05f){for(auto& [v,before]:edits)*v=before;return 0;}
        if(localShoulder){
            // A grafted arm end is not native torso cloth. Isolate the native
            // upper-arm subtree before transferring the shared distribution.
            for(auto it=mean.begin();it!=mean.end();){
                bool own=false;int b=static_cast<int>(it->first);
                for(std::size_t n=0;b>=0&&std::size_t(b)<rig.bones.size()&&n<rig.bones.size();++n){if(b==elbow)break;if(b==shoulder){own=true;break;}b=rig.bones[b].parent;}
                if(!own)it=mean.erase(it);else ++it;
            }
            if(mean.empty()){for(auto& [v,before]:edits)*v=before;return 0;}
        }
        std::vector<std::pair<uint32_t,float>> sorted(mean.begin(),mean.end());std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.second>b.second;});
        for(auto& [v,before]:edits){v->bones={};v->weights={};float norm=0;
            for(std::size_t k=0;k<std::min<std::size_t>(4,sorted.size());++k){v->bones[k]=sorted[k].first;v->weights[k]=sorted[k].second;norm+=sorted[k].second;}for(auto& w:v->weights)w/=norm;
        }
        if(transition){
            const auto common=*edits.front().first;
            for(auto& mesh:fitted)if(!mesh.viewmodelWeapon){
                std::vector<std::vector<uint32_t>> adjacent(mesh.vertices.size());bool valid=true;
                for(auto i:mesh.indices)if(i>=mesh.vertices.size())valid=false;
                if(!valid)continue;
                for(std::size_t t=0;t+2<mesh.indices.size();t+=3)for(int k=0;k<3;++k){auto a=mesh.indices[t+k],b=mesh.indices[t+(k+1)%3];adjacent[a].push_back(b);adjacent[b].push_back(a);}
                std::vector<bool> visited(mesh.vertices.size()),seed(mesh.vertices.size());std::vector<uint32_t> pending;
                for(std::size_t i=0;i<mesh.vertices.size();++i)for(const auto& edit:edits)if(edit.first==&mesh.vertices[i]){visited[i]=seed[i]=true;pending.push_back(static_cast<uint32_t>(i));break;}
                for(std::size_t q=0;q<pending.size();++q)for(auto next:adjacent[pending[q]])if(!visited[next]){
                    const auto& v=mesh.vertices[next];float mass=0;for(int k=0;k<4;++k)if(v.bones[k]==std::size_t(shoulder))mass+=v.weights[k];
                    const float t=dot(v.position-origin,axis)/length2;
                    if(mass<=0||t<-.1f||t>1.f)continue;
                    visited[next]=true;pending.push_back(next);
                }
                for(auto i:pending)if(!seed[i]){
                    auto& v=mesh.vertices[i];std::map<uint32_t,float> w;float mass=0;
                    for(int k=0;k<4;++k)if(v.weights[k]>0){if(v.bones[k]==std::size_t(shoulder))mass+=v.weights[k];else w[v.bones[k]]+=v.weights[k];}
                    for(int k=0;k<4;++k)if(common.weights[k]>0)w[common.bones[k]]+=mass*common.weights[k];
                    std::vector<std::pair<uint32_t,float>> ranked(w.begin(),w.end());std::sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.second>b.second;});
                    v.bones={};v.weights={};float total=0;for(std::size_t k=0;k<std::min<std::size_t>(4,ranked.size());++k){v.bones[k]=ranked[k].first;v.weights[k]=ranked[k].second;total+=v.weights[k];}for(auto& weight:v.weights)weight/=total;++changed;
                }
            }
        }
    }
    return changed;
}
}
