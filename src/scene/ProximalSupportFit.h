#pragma once
#include "scene/ImportedViewRetarget.h"
#include <map>
#include <numeric>
#include <queue>
#include <tuple>

namespace scene::proximal_support {
// Normal transport for p' = p + axis * (f(dot(p,axis))-dot(p,axis)).
// Cofactor form avoids dividing by a vanishing endpoint derivative. Axis
// must be unit length; the unchanged radial normal is the zero-derivative
// limit when its axial component is exactly zero.
inline Vec3 axialNormal(Vec3 normal,Vec3 axis,float derivative){
    if(!std::isfinite(derivative)||derivative<0)return normal;
    const auto parallel=axis*dot(normal,axis);
    const auto transported=(normal-parallel)*derivative+parallel;
    const float magnitude=length(transported);
    return std::isfinite(magnitude)&&magnitude>1e-12f?transported/magnitude:normal;
}
// Experimental skin-weight-only correction. Never enabled implicitly.
struct Options {bool enabled=false;float maxDistanceFraction=.15f,proximalFraction=.45f;bool conformSurface=false;};
struct Report {std::size_t candidates{},edited{},rejected{},badPalette{},eligibleTriangles{};};
inline float matrixError(const Mat4& a,const Mat4& b){float e=0;for(int k=0;k<16;++k)e=std::max(e,std::abs(a.v[k]-b.v[k]));return e;}
inline bool below(const Skeleton& s,int bone,int ancestor){for(std::size_t n=0;bone>=0&&static_cast<std::size_t>(bone)<s.bones.size()&&n<s.bones.size();++n){if(bone==ancestor)return true;bone=s.bones[bone].parent;}return false;}
struct Closest {Vec3 point;std::array<float,3> weights;};
inline Closest closest(Vec3 p,Vec3 a,Vec3 b,Vec3 c){
    const auto ab=b-a,ac=c-a,ap=p-a;const float d1=dot(ab,ap),d2=dot(ac,ap);
    if(d1<=0&&d2<=0)return {a,{1,0,0}};
    const auto bp=p-b;const float d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return {b,{0,1,0}};
    const float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0){const float v=d1/(d1-d3);return {a+ab*v,{1-v,v,0}};}
    const auto cp=p-c;const float d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return {c,{0,0,1}};
    const float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0){const float w=d2/(d2-d6);return {a+ac*w,{1-w,0,w}};}
    const float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){const float w=(d4-d3)/((d4-d3)+(d5-d6));return {b+(c-b)*w,{0,1-w,w}};}
    const float inverse=1/(va+vb+vc),v=vb*inverse,w=vc*inverse;return {a+ab*v+ac*w,{1-v-w,v,w}};
}
inline Vec3 surfaceNormal(const Mat4& model,Vec3 n,Vec3 fallback){
    const Vec3 x{model.v[0],model.v[1],model.v[2]},y{model.v[4],model.v[5],model.v[6]},z{model.v[8],model.v[9],model.v[10]};
    const float determinant=dot(x,cross(y,z));
    if(!std::isfinite(determinant)||std::abs(determinant)<1e-10f)return fallback;
    const auto transformed=(cross(y,z)*n.x+cross(z,x)*n.y+cross(x,y)*n.z)/determinant;
    const float magnitude=length(transformed);
    return std::isfinite(magnitude)&&magnitude>1e-8f?transformed/magnitude:fallback;
}
struct Triangle {std::array<Vec3,3> p,n;std::array<const Vertex*,3> v;Vec3 normal;std::size_t meshId{},triangleId{},componentId{};};
inline std::vector<Triangle> collect(const CastScene& native,int shoulder,int elbow,const std::vector<int>& driver,std::string& error,float minimumUpper=.5f){
    error.clear();std::vector<Triangle> triangles;const auto& skeleton=native.skeleton;
    if(shoulder<0||elbow<0||static_cast<std::size_t>(shoulder)>=driver.size()||static_cast<std::size_t>(elbow)>=driver.size()||driver.size()!=skeleton.bones.size()){error="Invalid support anatomy/palette";return triangles;}
    const auto origin=transformPoint(skeleton.bones[shoulder].restGlobal,{}),axis=transformPoint(skeleton.bones[elbow].restGlobal,{})-origin;const auto len=length(axis);if(len<1e-4f)return triangles;const auto direction=axis/len;
    std::vector<bool> allowed(driver.size());allowed[shoulder]=true;
    for(std::size_t b=0;b<allowed.size();++b){const auto name=assets::imported::lower(skeleton.bones[b].name);const bool helper=name.find("twist")!=std::string::npos||name.find("roll")!=std::string::npos||name.find("bulge")!=std::string::npos;const auto delta=transformPoint(skeleton.bones[b].restGlobal,{})-origin;const float along=dot(delta,direction);if(helper&&below(skeleton,static_cast<int>(b),shoulder)&&!below(skeleton,static_cast<int>(b),elbow)&&along>=-.1f*len&&along<=1.1f*len&&length(delta-direction*along)<=.2f*len)allowed[b]=true;}
    int parent=skeleton.bones[shoulder].parent;for(int depth=0;parent>=0&&static_cast<std::size_t>(parent)<allowed.size()&&depth<2;++depth){allowed[parent]=true;parent=skeleton.bones[parent].parent;}
    for(std::size_t mi=0;mi<native.meshes.size();++mi){const auto& mesh=native.meshes[mi];if(mesh.viewmodelWeapon||!mesh.skinned||mesh.attachmentIndex>=0)continue;
        std::vector<std::size_t> components(mesh.vertices.size());std::iota(components.begin(),components.end(),0);
        const auto root=[&](std::size_t i){while(components[i]!=i){components[i]=components[components[i]];i=components[i];}return i;};
        for(std::size_t t=0;t+2<mesh.indices.size();t+=3){for(int k=0;k<3;++k)if(mesh.indices[t+k]>=components.size()){error="Invalid native support topology";return {};}const auto r=root(mesh.indices[t]);components[root(mesh.indices[t+1])]=r;components[root(mesh.indices[t+2])]=r;}
        std::vector<bool> distal(components.size());for(std::size_t i=0;i<mesh.vertices.size();++i){float w=0;const auto& v=mesh.vertices[i];for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<driver.size()&&below(skeleton,static_cast<int>(v.bones[k]),elbow))w+=v.weights[k];if(w>.25f)distal[root(i)]=true;}
        for(std::size_t t=0;t+2<mesh.indices.size();t+=3){if(!distal[root(mesh.indices[t])])continue;Triangle tri;tri.meshId=mi;tri.triangleId=t/3;tri.componentId=root(mesh.indices[t]);bool valid=true;
            for(int c=0;c<3;++c){const auto& v=mesh.vertices[mesh.indices[t+c]];tri.v[c]=&v;tri.p[c]=transformPoint(mesh.modelTransform,v.position);float sum=0,upper=0;for(int k=0;k<4;++k)if(v.weights[k]>0){const auto b=v.bones[k];if(b>=driver.size()||driver[b]<0||!allowed[b]){valid=false;break;}sum+=v.weights[k];if(below(skeleton,static_cast<int>(b),shoulder))upper+=v.weights[k];}valid&=sum>.999f&&sum<1.001f&&upper>minimumUpper;}
            const auto normal=cross(tri.p[1]-tri.p[0],tri.p[2]-tri.p[0]);if(valid&&length(normal)>1e-6f){tri.normal=normalize(normal);for(int c=0;c<3;++c)tri.n[c]=surfaceNormal(mesh.modelTransform,tri.v[c]->normal,tri.normal);triangles.push_back(tri);}
        }
    }return triangles;
}
inline void preserveSeams(std::vector<Vertex>& vertices,const std::vector<Vertex>& original,float tolerance){
    std::vector<std::size_t> order(original.size());std::iota(order.begin(),order.end(),0);
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return original[a].position.x<original[b].position.x;});
    std::vector<bool> visited(original.size());
    for(std::size_t i=0;i<order.size();++i){const auto anchor=order[i];if(visited[anchor])continue;
        std::vector<std::size_t> group;std::size_t donor=original.size();
        for(std::size_t j=i;j<order.size();++j){const auto id=order[j];if(original[id].position.x-original[anchor].position.x>tolerance)break;
            if(visited[id]||original[id].bones!=original[anchor].bones||original[id].weights!=original[anchor].weights||length(original[id].position-original[anchor].position)>tolerance)continue;
            group.push_back(id);visited[id]=true;
            if(donor==original.size()&&(vertices[id].bones!=original[id].bones||vertices[id].weights!=original[id].weights))donor=id;
        }
        if(group.size()<2||donor==original.size())continue;
        const auto displacement=vertices[donor].position-original[donor].position;
        const auto bones=vertices[donor].bones;const auto weights=vertices[donor].weights;
        for(auto id:group){vertices[id].position=original[id].position+displacement;vertices[id].bones=bones;vertices[id].weights=weights;}
        // Keep UVs and hard-edge normals distinct; only geometric support is shared.
    }
}
// Explicit diagnostic entry point, separate from legacy follower fitting.
// Transfers only weights; fitted bind geometry and distal skin stay intact.
inline bool coherentConfidence(const Mesh& mesh,const std::vector<bool>& matched,const std::vector<float>& raw,std::vector<float>& result){
    const auto n=mesh.vertices.size();if(matched.size()!=n||raw.size()!=n||mesh.indices.size()%3)return false;
    std::vector<std::map<std::size_t,float>> neighbors(n);
    for(std::size_t t=0;t<mesh.indices.size();t+=3){
        for(int k=0;k<3;++k)if(mesh.indices[t+k]>=n)return false;
        for(int k=0;k<3;++k){const auto a=mesh.indices[t+k],b=mesh.indices[t+(k+1)%3];if(a==b)continue;
            const float d=length(mesh.vertices[a].position-mesh.vertices[b].position);if(!std::isfinite(d))return false;
            const float weight=1/std::max(d,1e-5f);neighbors[a][b]=weight;neighbors[b][a]=weight;}
    }
    result.assign(n,0);std::vector<bool> fixed(n);
    for(std::size_t i=0;i<n;++i){if(!std::isfinite(raw[i])||raw[i]<0||raw[i]>1)return false;fixed[i]=!matched[i]||raw[i]>=.8f;if(matched[i]&&fixed[i])result[i]=raw[i];}
    // Harmonic extension only through already validated surface matches.
    // Rejected vertices are zero boundaries; disconnected unanchored islands
    // remain zero. No new native correspondence or geometry is synthesized.
    for(int iteration=0;iteration<128;++iteration){auto next=result;float change=0;
        for(std::size_t i=0;i<n;++i)if(!fixed[i]&&!neighbors[i].empty()){
            double sum=0,total=0;for(const auto& [j,w]:neighbors[i]){sum+=w*result[j];total+=w;}
            next[i]=static_cast<float>(sum/total);change=std::max(change,std::abs(next[i]-result[i]));}
        result.swap(next);if(change<1e-6f)break;
    }
    return true;
}
// Diagnostic only: fill short unmatched holes using fixed, already transferred
// same-arm support. Exact original seams are one graph node. No geometry edits.
inline bool extendSupportHoles(Mesh& mesh,const std::vector<Vertex>& original,const std::vector<bool>& eligible,float radius){
    const auto n=mesh.vertices.size();if(original.size()!=n||eligible.size()!=n||mesh.indices.size()%3||!std::isfinite(radius)||radius<=0)return false;
    using Weights=std::map<uint32_t,double>;
    const auto palette=[](const Vertex& v){Weights w;for(int k=0;k<4;++k)if(v.weights[k]>0)w[v.bones[k]]+=v.weights[k];return w;};
    using Key=std::tuple<float,float,float,std::array<uint32_t,4>,std::array<float,4>>;
    std::map<Key,std::size_t> seams;std::vector<std::size_t> rep(n);std::vector<Weights> values(n);
    std::vector<bool> anchor(n),unknown(n);
    for(std::size_t i=0;i<n;++i){const auto& v=original[i];if(!std::isfinite(v.position.x)||!std::isfinite(v.position.y)||!std::isfinite(v.position.z))return false;
        const Key key{v.position.x,v.position.y,v.position.z,v.bones,v.weights};auto [it,inserted]=seams.emplace(key,i);rep[i]=it->second;
        values[i]=palette(mesh.vertices[i]);anchor[i]=eligible[i]&&values[i]!=palette(v);
        if(!inserted&&(eligible[i]!=eligible[rep[i]]||values[i]!=values[rep[i]]))return false;
    }
    std::vector<std::map<std::size_t,float>> edges(n);
    for(std::size_t t=0;t<mesh.indices.size();t+=3){for(int k=0;k<3;++k)if(mesh.indices[t+k]>=n)return false;
        for(int k=0;k<3;++k){const auto a=rep[mesh.indices[t+k]],b=rep[mesh.indices[t+(k+1)%3]];if(a==b)continue;
            const float d=length(original[a].position-original[b].position);edges[a][b]=d;edges[b][a]=d;}}
    std::vector<float> distance(n,std::numeric_limits<float>::infinity());
    using Entry=std::pair<float,std::size_t>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;
    for(std::size_t i=0;i<n;++i)if(rep[i]==i&&anchor[i]){distance[i]=0;queue.push({0.f,i});}
    while(!queue.empty()){const auto [d,i]=queue.top();queue.pop();if(d!=distance[i])continue;
        for(const auto& [j,e]:edges[i])if(eligible[j]&&d+e<distance[j]&&d+e<=radius){distance[j]=d+e;queue.push({d+e,j});}}
    for(std::size_t i=0;i<n;++i)unknown[i]=eligible[i]&&!anchor[i]&&distance[rep[i]]<=radius;
    bool converged=false;
    for(int iteration=0;iteration<512;++iteration){auto next=values;double change=0;
        for(std::size_t i=0;i<n;++i)if(rep[i]==i&&unknown[i]){Weights w;double total=0;
            for(const auto& [j,d]:edges[i])if(eligible[j]&&(anchor[j]||unknown[j])){const double weight=1/std::max(d,.05f);total+=weight;for(const auto& [b,v]:values[j])w[b]+=weight*v;}
            if(total<=0)return false;for(auto& [b,v]:w)v/=total;
            for(const auto& [b,v]:w)change=std::max(change,std::abs(v-(values[i].contains(b)?values[i].at(b):0)));
            for(const auto& [b,v]:values[i])if(!w.contains(b))change=std::max(change,std::abs(v));next[i]=std::move(w);}
        for(std::size_t i=0;i<n;++i)if(unknown[i])next[i]=next[rep[i]];values.swap(next);
        if(change<1e-7){converged=true;break;}}
    if(!converged)return false;
    // Validate the whole correction before changing any palette.
    for(std::size_t i=0;i<n;++i)if(unknown[i]){std::erase_if(values[i],[](auto w){return w.second<=1e-7;});double sum=0;for(const auto& [b,v]:values[i]){if(!std::isfinite(v)||v<0)return false;sum+=v;}if(values[i].size()>4||std::abs(sum-1)>.001)return false;}
    for(std::size_t i=0;i<n;++i)if(unknown[i]){auto& v=mesh.vertices[i];v.bones={};v.weights={};double sum=0;for(const auto& w:values[i])sum+=w.second;int k=0;for(const auto& [bone,weight]:values[i]){v.bones[k]=bone;v.weights[k++]=static_cast<float>(weight/sum);}}
    return true;
}
inline bool applyDirectNative(CastScene& fitted,const CastScene& native,Report& report,std::string& error,bool coherent=false,bool holes=false,bool fullMatched=false){
    report={};error.clear();
    if(native.importedViewGame=="eldewrito"&&native.viewHandsDriverGame.empty()){error="Renderer-corrected native palette unsupported";return false;}
    const auto layout=imported::viewLayout(native);
    if(!layout.valid()){error="Direct support requires complete native arms";return false;}
    std::vector<int> driver(native.skeleton.bones.size(),-1);
    for(std::size_t b=0;b<driver.size();++b){
        const auto it=fitted.skeleton.boneByName.find(native.skeleton.bones[b].name);
        if(it==fitted.skeleton.boneByName.end()||it->second>=fitted.skeleton.bones.size())continue;
        const auto i=it->second;
        if(matrixError(fitted.skeleton.bones[i].restGlobal,native.skeleton.bones[b].restGlobal)>.001f||matrixError(fitted.skeleton.bones[i].inverseBind,inverseAffine(native.skeleton.bones[b].restGlobal))>.001f)continue;
        driver[b]=static_cast<int>(i);
    }
    for(const auto& mesh:fitted.meshes)if(!mesh.viewmodelWeapon&&mesh.skinned&&(mesh.attachmentIndex>=0||matrixError(mesh.modelTransform,Mat4::identity())>.00001f)){error="Direct support requires scene-space skin";return false;}
    const auto smooth=[](float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);};
    for(const auto& arm:layout.arms){
        if(arm.shoulder<0||arm.elbow<0||driver[arm.shoulder]<0||driver[arm.elbow]<0)continue;
        const auto origin=transformPoint(native.skeleton.bones[arm.shoulder].restGlobal,{});
        const auto delta=transformPoint(native.skeleton.bones[arm.elbow].restGlobal,{})-origin;
        const float armLength=length(delta);if(armLength<1e-4f)continue;const auto axis=delta/armLength;
        std::vector<bool> eligible(fitted.skeleton.bones.size());eligible[driver[arm.shoulder]]=true;
        for(std::size_t b=0;b<driver.size();++b){
            if(driver[b]<0||!below(native.skeleton,int(b),arm.shoulder)||below(native.skeleton,int(b),arm.elbow))continue;
            const auto name=assets::imported::lower(native.skeleton.bones[b].name);
            const auto d=transformPoint(native.skeleton.bones[b].restGlobal,{})-origin;const float along=dot(d,axis);
            if((name.find("twist")!=std::string::npos||name.find("roll")!=std::string::npos||name.find("bulge")!=std::string::npos)&&along>=-.1f*armLength&&along<=1.1f*armLength&&length(d-axis*along)<=.2f*armLength)eligible[driver[b]]=true;
        }
        const auto triangles=collect(native,arm.shoulder,arm.elbow,driver,error,.05f);if(!error.empty())return false;
        report.eligibleTriangles+=triangles.size();
        for(auto& mesh:fitted.meshes){if(mesh.viewmodelWeapon||!mesh.skinned)continue;const auto original=mesh.vertices;
            struct Match {const Triangle* triangle{};Closest point{};float axialFade{};};
            std::vector<Match> matches(mesh.vertices.size());std::vector<bool> matched(mesh.vertices.size()),supportEligible(mesh.vertices.size());std::vector<float> confidences(mesh.vertices.size());
            for(std::size_t vi=0;vi<mesh.vertices.size();++vi){const auto& v=mesh.vertices[vi];float sum=0,unsupported=0;bool valid=true;
                // Fitted blend curves can leave millionth-scale elbow tails on
                // otherwise rigid proximal vertices. Treat their TOTAL as
                // numerical residue, not as a genuine distal skin region.
                for(int k=0;k<4;++k){if(!std::isfinite(v.weights[k])||v.weights[k]<0){valid=false;break;}if(v.weights[k]>0){sum+=v.weights[k];if(v.bones[k]>=eligible.size()){valid=false;break;}if(!eligible[v.bones[k]])unsupported+=v.weights[k];}}
                if(!valid||unsupported>1e-5f||sum<.999f||sum>1.001f)continue;
                const float axial=dot(v.position-origin,axis)/armLength;
                if(axial<-.15f||axial>=.45f)continue;++report.candidates;supportEligible[vi]=true;
                float distance=.15f*armLength;const Triangle* best=nullptr;Closest match{};
                for(const auto& tri:triangles){if(dot(normalize(v.normal),tri.normal)<.2f)continue;
                    const auto q=closest(v.position,tri.p[0],tri.p[1],tri.p[2]);const float d=length(q.point-v.position);
                    if(d<distance){distance=d;best=&tri;match=q;}}
                if(!best){++report.rejected;continue;}
                matched[vi]=true;matches[vi]={best,match,1-smooth(axial/.45f)};
                confidences[vi]=1-smooth((distance/(.15f*armLength)-.8f)/.2f);
            }
            // Explicit diagnostic alternative: distance remains a correspondence
            // rejection gate, not a per-vertex blend toward rigid shoulder skin.
            // Keep the existing axial transition and every anatomy/normal guard.
            if(fullMatched){for(std::size_t i=0;i<matched.size();++i)if(matched[i])confidences[i]=1;}
            else if(coherent){std::vector<float> field;if(!coherentConfidence(mesh,matched,confidences,field)){error="Invalid direct-support confidence topology";return false;}confidences.swap(field);}
            for(std::size_t vi=0;vi<mesh.vertices.size();++vi){if(!matched[vi])continue;auto& v=mesh.vertices[vi];
                const auto* best=matches[vi].triangle;const auto& match=matches[vi].point;
                const float confidence=confidences[vi]*matches[vi].axialFade;
                std::map<uint32_t,float> weights;
                for(int k=0;k<4;++k)if(v.weights[k]>0)weights[v.bones[k]]+=(1-confidence)*v.weights[k];
                for(int c=0;c<3;++c)for(int k=0;k<4;++k)if(best->v[c]->weights[k]>0)weights[driver[best->v[c]->bones[k]]]+=confidence*match.weights[c]*best->v[c]->weights[k];
                std::erase_if(weights,[](const auto& w){return w.second<=1e-5f;});
                float total=0;for(const auto& w:weights)total+=w.second;
                if(weights.size()>4||!std::isfinite(total)||total<.999f||total>1.001f){++report.badPalette;continue;}
                v.bones={};v.weights={};int k=0;for(const auto& [bone,weight]:weights){v.bones[k]=bone;v.weights[k++]=weight/total;}++report.edited;
            }
            preserveSeams(mesh.vertices,original,armLength*.00025f);
            if(holes&&!extendSupportHoles(mesh,original,supportEligible,.15f*armLength)){error="Invalid bounded support-hole correction";return false;}
        }
    }
    return true;
}
inline bool apply(CastScene& fitted,const CastScene& native,const Options& options,Report& report,std::string& error){
    report={};error.clear();if(!options.enabled)return true;
    if(!std::isfinite(options.maxDistanceFraction)||options.maxDistanceFraction<=0||!std::isfinite(options.proximalFraction)||options.proximalFraction<=0||options.proximalFraction>=1){error="Invalid proximal support limits";return false;}
    if(!fitted.codmRigAdapter){error="Proximal support requires a verified legacy follower palette";return false;}
    // Renderer-only corrections would need an additional equivalence proof.
    if(native.importedViewGame=="eldewrito"&&native.viewHandsDriverGame.empty()){error="Renderer-corrected native palette unsupported by this candidate";return false;}
    const auto layout=imported::viewLayout(native);if(!layout.valid()){error="Proximal support requires complete native arms";return false;}
    std::vector<int> driver(native.skeleton.bones.size(),-1);
    for(std::size_t b=0;b<driver.size();++b){const auto it=fitted.skeleton.boneByName.find(native.skeleton.bones[b].name);if(it==fitted.skeleton.boneByName.end())continue;const auto i=it->second;
        if(i>=fitted.codmRigAdapter->firstBone||matrixError(fitted.skeleton.bones[i].restGlobal,native.skeleton.bones[b].restGlobal)>.001f||matrixError(fitted.skeleton.bones[i].inverseBind,inverseAffine(native.skeleton.bones[b].restGlobal))>.001f)continue;driver[b]=static_cast<int>(i);}
    for(const auto& mesh:fitted.meshes)if(!mesh.viewmodelWeapon&&mesh.skinned&&(mesh.attachmentIndex>=0||matrixError(mesh.modelTransform,Mat4::identity())>.00001f)){error="Fitted support skin must use identity scene-space model transform";return false;}
    for(const auto& arm:layout.arms){if(arm.shoulder<0||arm.elbow<0||driver[arm.shoulder]<0)continue;
        const auto origin=transformPoint(native.skeleton.bones[arm.shoulder].restGlobal,{}),axis=transformPoint(native.skeleton.bones[arm.elbow].restGlobal,{})-origin;const float armLength=length(axis);if(armLength<1e-4f)continue;const auto direction=axis/armLength;
        const auto triangles=collect(native,arm.shoulder,arm.elbow,driver,error);if(!error.empty())return false;
        report.eligibleTriangles+=triangles.size();if(triangles.empty())continue;
        for(auto& mesh:fitted.meshes){if(mesh.viewmodelWeapon||!mesh.skinned)continue;if(mesh.attachmentIndex>=0||matrixError(mesh.modelTransform,Mat4::identity())>.00001f){error="Fitted support skin must use identity scene-space model transform";return false;}
            const auto original=options.conformSurface?mesh.vertices:std::vector<Vertex>{};
            for(auto& vertex:mesh.vertices){float sum=0;bool shoulderOnly=true;
                for(int k=0;k<4;++k)if(vertex.weights[k]>0){sum+=vertex.weights[k];const auto b=vertex.bones[k];if(b<fitted.codmRigAdapter->firstBone||b-fitted.codmRigAdapter->firstBone>=fitted.codmRigAdapter->bindings.size()){shoulderOnly=false;break;}const auto& binding=fitted.codmRigAdapter->bindings[b-fitted.codmRigAdapter->firstBone];
                    if(binding.source!=static_cast<std::size_t>(driver[arm.shoulder])||binding.rollWeight!=0||b>=fitted.skeleton.bones.size()||matrixError(binding.offset*fitted.skeleton.bones[b].inverseBind,fitted.skeleton.bones[binding.source].inverseBind)>.001f){shoulderOnly=false;break;}}
                if(!shoulderOnly||sum<.999f||sum>1.001f)continue;
                const float axial=dot(vertex.position-origin,direction)/armLength;if(axial<0||axial>=options.proximalFraction)continue;++report.candidates;
                const Triangle* best=nullptr;Closest match{};float distance=options.maxDistanceFraction*armLength;
                for(const auto& tri:triangles){if(dot(normalize(vertex.normal),tri.normal)<.2f)continue;const auto q=closest(vertex.position,tri.p[0],tri.p[1],tri.p[2]);const float d=length(q.point-vertex.position);if(d<distance){distance=d;best=&tri;match=q;}}
                if(!best){++report.rejected;continue;}
                const auto smooth=[](float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);};
                const float confidence=options.conformSurface
                    ? (1-smooth((distance/(options.maxDistanceFraction*armLength)-.5f)*2))*(1-smooth((axial/options.proximalFraction-.5f)*2))
                    : (1-smooth(distance/(options.maxDistanceFraction*armLength)))*(1-smooth(axial/options.proximalFraction));
                std::map<uint32_t,float> weights;weights[static_cast<uint32_t>(driver[arm.shoulder])]=1-confidence;
                for(int c=0;c<3;++c)for(int k=0;k<4;++k)if(best->v[c]->weights[k]>0)weights[static_cast<uint32_t>(driver[best->v[c]->bones[k]])]+=confidence*match.weights[c]*best->v[c]->weights[k];
                std::erase_if(weights,[](const auto& entry){return entry.second<=1e-7f;});if(weights.size()>4){++report.badPalette;continue;}
                float total=0;for(const auto& entry:weights)total+=entry.second;if(!std::isfinite(total)||total<.999f||total>1.001f){++report.badPalette;continue;}
                vertex.bones={};vertex.weights={};int k=0;for(const auto& [bone,weight]:weights){vertex.bones[k]=bone;vertex.weights[k++]=weight/total;}
                if(options.conformSurface){
                    // Diagnostic bind-space surface fit, not camera-dependent
                    // clipping. Preserve topology/UVs and the distal arm.
                    vertex.position+=(match.point-vertex.position)*confidence;
                    auto interpolated=best->n[0]*match.weights[0]+best->n[1]*match.weights[1]+best->n[2]*match.weights[2];
                    const float magnitude=length(interpolated);
                    interpolated=std::isfinite(magnitude)&&magnitude>1e-8f?interpolated/magnitude:best->normal;
                    vertex.normal=normalize(vertex.normal*(1-confidence)+interpolated*confidence);
                }
                ++report.edited;
            }
            if(options.conformSurface)preserveSeams(mesh.vertices,original,armLength*.00025f);
        }
    }
    return true;
}
}
