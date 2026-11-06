#pragma once
#include "scene/CastScene.h"
#include <map>
#include <set>
#include <tuple>

namespace scene::imported {
struct ProximalArm { int shoulder{-1},elbow{-1},wrist{-1}; bool donorShoulderless{}; };
struct ProximalResult { unsigned modifiedMeshes{},addedVertices{}; bool requiresTextureReview{}; std::string reason; };

// Opt-in geometry experiment. No camera/clip dependence, no source attributes
// changed. Texture continuity and anatomical fidelity still require review.
inline ProximalResult continueProximalDiagnostic(std::vector<Mesh>& meshes,const Skeleton& rig,
                                                const ProximalArm& arm,bool enabled=false,bool surfaceUv=false,bool localTopology=false){
    ProximalResult result;
    const auto valid=[&](int b){return b>=0&&static_cast<size_t>(b)<rig.bones.size();};
    if(!enabled){result.reason="disabled";return result;}
    if(!arm.donorShoulderless||!valid(arm.shoulder)||!valid(arm.elbow)||!valid(arm.wrist)){result.reason="ineligible anatomy";return result;}
    const auto pos=[&](int b){return transformPoint(rig.bones[b].restGlobal,{});};
    const auto elbow=pos(arm.elbow),shoulder=pos(arm.shoulder),fore=pos(arm.wrist)-elbow;
    const float lengthFore=length(fore),lengthUpper=length(shoulder-elbow);
    if(!std::isfinite(lengthFore)||!std::isfinite(lengthUpper)||lengthFore<1e-4f||lengthUpper<1e-4f){result.reason="degenerate anatomy";return result;}
    const auto axis=fore/lengthFore;const float epsilon=lengthFore*1e-5f;
    struct Candidate {size_t mesh;std::vector<std::pair<uint32_t,uint32_t>> edges;Vec3 center;};
    std::vector<Candidate> candidates;
    unsigned rejectedAttributes=0,rejectedTopology=0,closedLoops=0,rejectedGeometry=0;
    for(size_t mi=0;mi<meshes.size();++mi){const auto& m=meshes[mi];if(m.viewmodelWeapon||!m.skinned)continue;
        if(m.modelTransform.v!=Mat4::identity().v)continue; // fitted mesh coordinates only
        std::map<std::tuple<long long,long long,long long>,uint32_t> welded;
        std::vector<uint32_t> ids(m.vertices.size());bool malformed=false;
        const auto compatible=[&](const Vertex& a,const Vertex& b){for(int k=0;k<4;++k)if(a.weights[k]>.001f){float weight=0;for(int j=0;j<4;++j)if(a.bones[k]==b.bones[j])weight+=b.weights[j];if(std::abs(weight-a.weights[k])>.001f)return false;}return true;};
        for(size_t i=0;i<m.vertices.size();++i){const auto& v=m.vertices[i];const auto p=v.position;if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)){malformed=true;break;}
            if(!std::isfinite(v.normal.x)||!std::isfinite(v.normal.y)||!std::isfinite(v.normal.z)||length(v.normal)<1e-6f||!std::isfinite(v.uv.x)||!std::isfinite(v.uv.y)){malformed=true;break;}
            float weightSum=0;for(int k=0;k<4;++k){if(!std::isfinite(v.weights[k])||v.weights[k]<0||(v.weights[k]>0&&v.bones[k]>=rig.bones.size())){malformed=true;break;}weightSum+=v.weights[k];}
            if(malformed||!std::isfinite(weightSum)||std::abs(weightSum-1.f)>.001f){malformed=true;break;}
            const auto key=std::make_tuple(std::llround(p.x/epsilon),std::llround(p.y/epsilon),std::llround(p.z/epsilon));auto [it,added]=welded.emplace(key,static_cast<uint32_t>(i));ids[i]=it->second;
            if(!added&&(!compatible(v,m.vertices[it->second])||!compatible(m.vertices[it->second],v))){malformed=true;break;}}
        if(malformed){++rejectedAttributes;continue;}
        using Edge=std::pair<uint32_t,uint32_t>;std::map<Edge,unsigned> incidence;std::map<Edge,Edge> corners;
        std::set<uint32_t> nonmanifold;
        for(size_t t=0;t+2<m.indices.size();t+=3)for(int k=0;k<3;++k){const auto a=m.indices[t+k],b=m.indices[t+(k+1)%3];if(a>=ids.size()||b>=ids.size()){malformed=true;continue;}auto x=ids[a],y=ids[b];if(x==y){malformed=true;continue;}if(x>y)std::swap(x,y);const Edge e{x,y};if(++incidence[e]>2){nonmanifold.insert(x);nonmanifold.insert(y);if(!localTopology)malformed=true;}corners[e]={a,b};}
        if(malformed){++rejectedTopology;continue;}
        std::map<uint32_t,std::vector<uint32_t>> adjacent;for(const auto& [e,n]:incidence)if(n==1){adjacent[e.first].push_back(e.second);adjacent[e.second].push_back(e.first);}
        std::set<uint32_t> seen;
        for(const auto& [start,neighbors]:adjacent){if(seen.contains(start))continue;std::vector<uint32_t> stack{start},loop;bool closed=true;
            while(!stack.empty()){const auto i=stack.back();stack.pop_back();if(!seen.insert(i).second)continue;loop.push_back(i);if(adjacent[i].size()!=2)closed=false;for(auto j:adjacent[i])if(!seen.contains(j))stack.push_back(j);}
            if(!closed||loop.size()<5||loop.size()>128)continue;
            if(localTopology){
                // Extension only reads this rim and its immediate UV neighbors.
                // Preserve unrelated nonmanifold hand geometry, but never extend
                // a rim whose incident source triangles touch ambiguous edges.
                const std::set<uint32_t> ring(loop.begin(),loop.end());
                for(size_t t=0;t+2<m.indices.size();t+=3){bool touches=false,unsafe=false;
                    for(int k=0;k<3;++k){const auto id=ids[m.indices[t+k]];touches|=ring.contains(id);unsafe|=nonmanifold.contains(id);}
                    if(touches&&unsafe){closed=false;break;}}
                if(!closed)continue;
            }
            ++closedLoops;
            Vec3 center{};bool eligible=true;for(auto i:loop){const auto& v=m.vertices[i];const float axial=dot(v.position-elbow,axis)/lengthFore;float lower=0;for(int k=0;k<4;++k)if(v.weights[k]>.001f){if(v.bones[k]==static_cast<uint32_t>(arm.elbow))lower+=v.weights[k];else if(v.bones[k]!=static_cast<uint32_t>(arm.wrist))eligible=false;}
                if(lower<.75f||axial<-.35f||axial>.15f)eligible=false;center+=v.position;}
            center=center/float(loop.size());const auto offset=center-elbow;const float radiusOffset=length(offset-axis*dot(offset,axis));float radius=0;for(auto i:loop)radius+=length(m.vertices[i].position-center);radius/=float(loop.size());
            if(!eligible||radius<lengthFore*.025f||radius>lengthFore*.5f||radiusOffset>radius*.5f){++rejectedGeometry;continue;}
            Candidate c{mi,{},center};std::set<uint32_t> component(loop.begin(),loop.end());for(const auto& [e,n]:incidence)if(n==1&&component.contains(e.first)&&component.contains(e.second))c.edges.push_back(corners[e]);candidates.push_back(std::move(c));
        }
    }
    if(candidates.size()!=1){result.reason=candidates.empty()?"no eligible proximal rim":"ambiguous proximal rims";
        result.reason+="; attribute_rejects="+std::to_string(rejectedAttributes)+" topology_rejects="+std::to_string(rejectedTopology)+" closed_loops="+std::to_string(closedLoops)+" geometry_rejects="+std::to_string(rejectedGeometry);return result;}
    const auto c=candidates.front();auto& m=meshes[c.mesh];const auto original=m.vertices;
    const auto startTangent=axis*-1.f,endTangent=normalize(shoulder-elbow);
    if(dot(startTangent,endTangent)<-.95f){result.reason="ambiguous reversed upper arm";return result;}
    const float span=length(shoulder-c.center);if(span<epsilon){result.reason="zero extension";return result;}
    const auto rotate=[&](Vec3 v,Vec3 toward){const float cosine=std::clamp(dot(startTangent,toward),-1.f,1.f);auto turn=cross(startTangent,toward);if(cosine<-.9999f)return v;const auto q=normalize(Quat{turn.x,turn.y,turn.z,1+cosine});return transformPoint(rotation(q),v);};
    struct UvStrip {Vec2 delta;float depth{};};std::map<uint32_t,UvStrip> strips;
    float stripDepth=lengthFore*.35f;
    if(surfaceUv){
        // Only reuse a segment actually present on the original forearm's
        // surface. Original corner indices keep UV seams on their own islands.
        // Never extrapolate a UV derivative into unverified atlas pixels.
        std::map<uint32_t,std::set<uint32_t>> adjacent;
        for(size_t i=0;i+2<m.indices.size();i+=3)for(int k=0;k<3;++k){auto a=m.indices[i+k],b=m.indices[i+(k+1)%3];adjacent[a].insert(b);adjacent[b].insert(a);}
        const auto lowerWeight=[&](const Vertex& v){float lower=0;for(int k=0;k<4;++k)if(v.weights[k]>.001f){if(v.bones[k]==static_cast<uint32_t>(arm.elbow))lower+=v.weights[k];else if(v.bones[k]!=static_cast<uint32_t>(arm.wrist))return -1.f;}return lower;};
        for(const auto& edge:c.edges)for(const auto id:{edge.first,edge.second})if(!strips.contains(id)){
            const auto& v=original[id];const float initialLower=lowerWeight(v);float bestScore=-1;UvStrip best;
            for(const auto neighbor:adjacent[id]){const auto& n=original[neighbor];const float endLower=lowerWeight(n);if(endLower<0)continue;
                const auto delta=n.position-v.position;const float axial=dot(delta,axis);if(axial<lengthFore*.04f)continue;
                const float lateral=length(delta-axis*axial);if(lateral>axial*.75f)continue;
                float fraction=std::min(1.f,lengthFore*.35f/axial);
                // The reused strip must remain predominately forearm, even if
                // its next mesh vertex is already weighted to the wrist.
                if(endLower<.75f){if(initialLower<=.75f)continue;fraction=std::min(fraction,(initialLower-.75f)/(initialLower-endLower));}
                const float depth=axial*fraction;const Vec2 duv{(n.uv.x-v.uv.x)*fraction,(n.uv.y-v.uv.y)*fraction};
                if(depth<lengthFore*.04f||duv.x*duv.x+duv.y*duv.y<1e-10f)continue;
                const float score=depth/(1+4*lateral/axial);if(score>bestScore){bestScore=score;best={duv,depth};}}
            if(bestScore<0){result.reason="no validated forearm UV strip";return result;}
            strips.emplace(id,best);stripDepth=std::min(stripDepth,best.depth);
        }
    }
    std::vector<float> ringFractions{.2f,.4f,.6f,.8f,1.f};
    if(surfaceUv){
        // Include every reflection endpoint and midpoint explicitly. Without
        // these rings, interpolation could collapse a whole repeated strip.
        const float step=stripDepth/span*.5f;
        if(step<1.f/128){result.reason="UV strip too short for bounded tessellation";return result;}
        for(float t=step;t<1.f;t+=step)ringFractions.push_back(t);
        std::sort(ringFractions.begin(),ringFractions.end());
        ringFractions.erase(std::unique(ringFractions.begin(),ringFractions.end(),[](float a,float b){return std::abs(a-b)<1e-5f;}),ringFractions.end());
    }
    // Separate corner strips preserve authored normal/UV seams. They share the
    // exact original rim and are welded geometrically at added seam duplicates.
    for(const auto& [a,b]:c.edges){std::array<uint32_t,2> previous{a,b};for(const float t:ringFractions){const float t2=t*t,t3=t2*t;
            const auto center=c.center*(2*t3-3*t2+1)+startTangent*(span*(t3-2*t2+t))+shoulder*(-2*t3+3*t2)+endTangent*(span*(t3-t2));
            const auto tangent=normalize(c.center*(6*t2-6*t)+startTangent*(span*(3*t2-4*t+1))+shoulder*(-6*t2+6*t)+endTangent*(span*(3*t2-2*t)));
            std::array<uint32_t,2> next{};for(int k=0;k<2;++k){const auto id=k?b:a;auto v=original[id];v.position=center+rotate(v.position-c.center,tangent)*(1-.05f*t);v.normal=normalize(rotate(v.normal,tangent));
                if(surfaceUv){const auto& strip=strips.at(id);const float period=stripDepth*2;const float cycle=std::fmod(t*span,period);const float depth=cycle<=stripDepth?cycle:period-cycle;
                    v.uv={v.uv.x+strip.delta.x*(depth/strip.depth),v.uv.y+strip.delta.y*(depth/strip.depth)};}
                const float shoulderWeight=t2*(3-2*t);float lower=0,wrist=0;for(int j=0;j<4;++j){if(v.bones[j]==static_cast<uint32_t>(arm.elbow))lower+=v.weights[j];else if(v.bones[j]==static_cast<uint32_t>(arm.wrist))wrist+=v.weights[j];}
                const float sum=lower+wrist;v.bones={static_cast<uint32_t>(arm.elbow),static_cast<uint32_t>(arm.shoulder),static_cast<uint32_t>(arm.wrist),0};v.weights={(1-shoulderWeight)*lower/sum,shoulderWeight,(1-shoulderWeight)*wrist/sum,0};
                next[k]=static_cast<uint32_t>(m.vertices.size());m.vertices.push_back(v);}
            m.indices.insert(m.indices.end(),{previous[1],previous[0],next[0],previous[1],next[0],next[1]});previous=next;}}
    result.modifiedMeshes=1;result.addedVertices=static_cast<unsigned>(m.vertices.size()-original.size());result.requiresTextureReview=true;result.reason=surfaceUv?"bounded mirrored source-surface UV strip; visual/material review required":"geometry diagnostic only; rim UV continuation is not material-approved";return result;
}

// Separate, opt-in experiment for a real upper arm with an exposed proximal
// rim. The virtual endpoint is used only to construct geometry; it is never
// inserted into the animation skeleton. Original skin attributes stay intact.
inline ProximalResult continueShoulderRimDiagnostic(std::vector<Mesh>& meshes,
        const Skeleton& rig,const ProximalArm& arm,bool enabled=false,bool surfaceUv=false,float extent=1.f){
    ProximalResult result;
    if(!enabled){result.reason="disabled";return result;}
    if(!std::isfinite(extent)||extent<=0||extent>1.f){result.reason="invalid proximal extent";return result;}
    if(arm.donorShoulderless||arm.shoulder<0||arm.elbow<0||
       static_cast<size_t>(arm.shoulder)>=rig.bones.size()||static_cast<size_t>(arm.elbow)>=rig.bones.size()){
        result.reason="ineligible shoulder anatomy";return result;
    }
    const auto shoulder=transformPoint(rig.bones[arm.shoulder].restGlobal,{});
    const auto elbow=transformPoint(rig.bones[arm.elbow].restGlobal,{});
    if(!std::isfinite(length(shoulder-elbow))||length(shoulder-elbow)<1e-4f){result.reason="degenerate upper arm";return result;}
    auto constructionRig=rig;Bone endpoint;
    endpoint.restGlobal=translation(shoulder+(shoulder-elbow)*extent);endpoint.inverseBind=inverseAffine(endpoint.restGlobal);
    const int endpointId=static_cast<int>(constructionRig.bones.size());constructionRig.bones.push_back(endpoint);
    std::vector<size_t> originalCounts;for(const auto& mesh:meshes)originalCounts.push_back(mesh.vertices.size());
    // Reuse closed-loop/manifold/attribute guards in a shoulder-centered frame.
    result=continueProximalDiagnostic(meshes,constructionRig,{endpointId,arm.shoulder,arm.elbow,true},true,surfaceUv,true);
    for(size_t m=0;m<meshes.size();++m)for(size_t i=originalCounts[m];i<meshes[m].vertices.size();++i){
        auto& v=meshes[m].vertices[i];float proximal=0,distal=0;
        for(int k=0;k<4;++k){if(v.bones[k]==static_cast<uint32_t>(arm.elbow))distal+=v.weights[k];else proximal+=v.weights[k];}
        v.bones={static_cast<uint32_t>(arm.shoulder),static_cast<uint32_t>(arm.elbow),0,0};v.weights={proximal,distal,0,0};
    }
    return result;
}
}
