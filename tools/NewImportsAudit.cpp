#include "assets/ImportedGamePolicy.h"
#include "scene/ImportedViewRetarget.h"
#include "scene/ImportedBodyRetarget.h"
#include "scene/CodmNative.h"
#include "scene/ColdWarLegacyBridge.h"
#include "scene/ProximalSupportFit.h"
#include "scene/PointBlankCodHands.h"
#include "render/NativeViewhandSkinning.h"
#include "scene/ArmSkinning.h"
#include <iostream>
#include <set>
#include <map>
#include <tuple>
// Diagnostic-only distal arm collector. Separate from the production proximal
// collector: cuff surfaces must admit elbow and semantic forearm helpers.
static std::vector<scene::proximal_support::Triangle> cuffSurfaces(const scene::CastScene& native,int shoulder,int elbow,int wrist){
    using namespace scene;std::vector<proximal_support::Triangle> out;const auto& s=native.skeleton;
    if(shoulder<0||elbow<0||wrist<0)return out;
    const auto origin=transformPoint(s.bones[shoulder].restGlobal,{}),axis=transformPoint(s.bones[elbow].restGlobal,{})-origin;
    const float len=length(axis);if(len<1e-4f)return out;const auto direction=axis/len;
    std::vector<bool> allowed(s.bones.size());allowed[shoulder]=allowed[elbow]=true;
    for(size_t b=0;b<s.bones.size();++b){const auto name=assets::imported::lower(s.bones[b].name);
        const bool helper=name.find("twist")!=std::string::npos||name.find("roll")!=std::string::npos||name.find("bulge")!=std::string::npos;
        if(helper&&proximal_support::below(s,int(b),shoulder)&&!proximal_support::below(s,int(b),wrist))allowed[b]=true;
    }
    for(size_t mi=0;mi<native.meshes.size();++mi){const auto& mesh=native.meshes[mi];if(mesh.viewmodelWeapon||!mesh.skinned||mesh.attachmentIndex>=0)continue;
        std::vector<size_t> component(mesh.vertices.size());std::iota(component.begin(),component.end(),0);
        const auto root=[&](size_t i){while(component[i]!=i){component[i]=component[component[i]];i=component[i];}return i;};
        bool validTopology=true;for(auto index:mesh.indices)if(index>=component.size())validTopology=false;if(!validTopology)continue;
        for(size_t t=0;t+2<mesh.indices.size();t+=3){const auto r=root(mesh.indices[t]);component[root(mesh.indices[t+1])]=r;component[root(mesh.indices[t+2])]=r;}
        std::vector<bool> proximal(component.size()),distal(component.size());
        for(size_t i=0;i<mesh.vertices.size();++i){const auto& v=mesh.vertices[i];float up=0,down=0;
            for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<allowed.size()&&allowed[v.bones[k]]){if(proximal_support::below(s,int(v.bones[k]),elbow))down+=v.weights[k];else up+=v.weights[k];}
            if(up>.5f)proximal[root(i)]=true;if(down>.5f)distal[root(i)]=true;
        }
        for(size_t t=0;t+2<mesh.indices.size();t+=3){const auto c=root(mesh.indices[t]);if(!proximal[c]||!distal[c])continue;
            proximal_support::Triangle tri;tri.meshId=mi;tri.triangleId=t/3;tri.componentId=c;bool valid=true;
            for(int j=0;j<3;++j){const auto& v=mesh.vertices[mesh.indices[t+j]];tri.v[j]=&v;tri.p[j]=transformPoint(mesh.modelTransform,v.position);float sum=0;
                for(int k=0;k<4;++k)if(v.weights[k]>0){if(v.bones[k]>=allowed.size()||!allowed[v.bones[k]])valid=false;sum+=v.weights[k];}
                const float axial=dot(tri.p[j]-origin,direction)/len;valid&=sum>.999f&&sum<1.001f&&axial>.45f&&axial<1.45f;
            }
            const auto n=cross(tri.p[1]-tri.p[0],tri.p[2]-tri.p[0]);if(valid&&length(n)>1e-6f){tri.normal=normalize(n);out.push_back(tri);}
        }
    }return out;
}
int main(int argc,char** argv){
    if(argc<2)return 2;std::string error;
    if(argc>=5&&std::string_view(argv[1])=="--raw-incidents"){
        std::size_t meshIndex;try{meshIndex=std::stoul(argv[3]);}catch(...){return 2;}
        const auto source=scene::buildScene(cast::Document::load(argv[2]),false);
        if(meshIndex>=source.meshes.size()){std::cerr<<"Mesh index out of range\n";return 3;}
        const auto& mesh=source.meshes[meshIndex];std::cout.precision(9);
        std::cout<<"raw_source="<<argv[2]<<" mesh="<<meshIndex<<" name="<<mesh.name<<" vertices="<<mesh.vertices.size()<<" triangles="<<mesh.indices.size()/3<<" model=";
        for(float x:mesh.modelTransform.v)std::cout<<x<<',';std::cout<<'\n';
        std::set<std::size_t> printedBones;
        const auto vertex=[&](std::size_t id){const auto& v=mesh.vertices.at(id);const auto p=scene::transformPoint(mesh.modelTransform,v.position);
            std::cout<<"vertex="<<id<<" local="<<v.position.x<<','<<v.position.y<<','<<v.position.z<<" bind="<<p.x<<','<<p.y<<','<<p.z<<" weights=";
            for(int k=0;k<4;++k)if(v.weights[k]>0){if(v.bones[k]>=source.skeleton.bones.size()){std::cout<<"INVALID;";continue;}std::cout<<v.bones[k]<<':'<<source.skeleton.bones[v.bones[k]].name<<':'<<v.weights[k]<<';';printedBones.insert(v.bones[k]);}std::cout<<'\n';
        };
        for(int argument=4;argument<argc;++argument){std::size_t id;try{id=std::stoul(argv[argument]);}catch(...){return 2;}if(id>=mesh.vertices.size())return 3;vertex(id);
            for(std::size_t t=0;t+2<mesh.indices.size();t+=3)if(mesh.indices[t]==id||mesh.indices[t+1]==id||mesh.indices[t+2]==id){std::cout<<"incident_triangle="<<t/3<<" indices="<<mesh.indices[t]<<','<<mesh.indices[t+1]<<','<<mesh.indices[t+2]<<'\n';for(int c=0;c<3;++c){const auto neighbor=mesh.indices[t+c];if(neighbor>=mesh.vertices.size())return 3;vertex(neighbor);std::cout<<"edge="<<id<<','<<neighbor<<" bind_length="<<scene::length(scene::transformPoint(mesh.modelTransform,mesh.vertices[id].position)-scene::transformPoint(mesh.modelTransform,mesh.vertices[neighbor].position))<<'\n';}}
        }
        for(auto b:printedBones){const auto& bone=source.skeleton.bones[b];std::cout<<"bone="<<b<<" name="<<bone.name<<" parent="<<bone.parent<<" rest=";for(float x:bone.restGlobal.v)std::cout<<x<<',';std::cout<<" inverse_bind=";for(float x:bone.inverseBind.v)std::cout<<x<<',';std::cout<<'\n';}
        return 0;
    }
    if(argc>=7&&(std::string_view(argv[1])=="--fit-native-surface"||std::string_view(argv[1])=="--fit-native-cuff"||std::string_view(argv[1])=="--legacy-native-surface")){
        const std::string sideText=argv[4];const int side=sideText=="left"?0:sideText=="right"?1:-1;if(side<0)return 2;
        const auto source=scene::buildScene(cast::Document::load(argv[2]),false),native=scene::buildScene(cast::Document::load(argv[3]),false);
        const bool legacy=std::string_view(argv[1])=="--legacy-native-surface",cuff=std::string_view(argv[1])=="--fit-native-cuff";
        auto fitted=native;if(!(legacy?scene::pointblank::fitToCod(fitted,cast::Document::load(argv[2]),error):scene::imported::fitForeignViewSkin(fitted,source,error))){std::cerr<<error;return 3;}
        const auto arm=scene::imported::viewLayout(native).arms[side];
        std::vector<int> mapping(native.skeleton.bones.size());std::iota(mapping.begin(),mapping.end(),0);
        const auto triangles=cuff?cuffSurfaces(native,arm.shoulder,arm.elbow,arm.wrist):scene::proximal_support::collect(native,arm.shoulder,arm.elbow,mapping,error);if(!error.empty()){std::cerr<<error;return 4;}
        std::cout<<"eligible_triangles="<<triangles.size()<<" side="<<side<<'\n';
        const auto origin=scene::transformPoint(native.skeleton.bones[arm.shoulder].restGlobal,{}),axis=scene::transformPoint(native.skeleton.bones[arm.elbow].restGlobal,{})-origin;
        for(int argument=6;argument<argc;++argument){std::size_t vertex;try{vertex=std::stoul(argv[argument]);}catch(...){return 2;}
            for(const auto& mesh:fitted.meshes)if(mesh.name.ends_with(argv[5])&&vertex<mesh.vertices.size()){
                const auto& v=mesh.vertices[vertex];const auto p=scene::transformPoint(mesh.modelTransform,v.position),normal=scene::normalize(scene::volume_fit::vector(scene::volume_fit::transpose3(scene::inverseAffine(mesh.modelTransform)),v.normal));
                std::cout<<"vertex="<<vertex<<" mesh="<<mesh.name<<" fitted="<<p.x<<','<<p.y<<','<<p.z<<" normal="<<normal.x<<','<<normal.y<<','<<normal.z<<" axial="<<scene::dot(p-origin,axis)/scene::dot(axis,axis)<<" weights=";
                for(int k=0;k<4;++k)if(v.weights[k]>0)std::cout<<fitted.skeleton.bones.at(v.bones[k]).name<<':'<<v.weights[k]<<';';std::cout<<'\n';
                if(legacy)for(size_t t=0;t+2<mesh.indices.size();t+=3){if(mesh.indices[t]!=vertex&&mesh.indices[t+1]!=vertex&&mesh.indices[t+2]!=vertex)continue;
                    std::cout<<"incident_triangle="<<t/3<<" neighbors=";
                    for(int c=0;c<3;++c){const auto i=mesh.indices[t+c];const auto& n=mesh.vertices.at(i);std::cout<<i<<":edge="<<scene::length(scene::transformPoint(mesh.modelTransform,n.position)-p)<<":weights=";for(int k=0;k<4;++k)if(n.weights[k]>0)std::cout<<fitted.skeleton.bones.at(n.bones[k]).name<<'/'<<n.weights[k]<<'|';std::cout<<';';}std::cout<<'\n';
                }
                for(bool normalGuard:{false,true}){
                    const scene::proximal_support::Triangle* best=nullptr;scene::proximal_support::Closest match{};float distance=std::numeric_limits<float>::max();
                    for(const auto& tri:triangles){if(normalGuard&&scene::dot(normal,tri.normal)<.2f)continue;const auto q=scene::proximal_support::closest(p,tri.p[0],tri.p[1],tri.p[2]);const float d=scene::length(p-q.point);if(d<distance){distance=d;match=q;best=&tri;}}
                    std::cout<<"normal_guard="<<normalGuard;if(!best){std::cout<<" no_match\n";continue;}
                    std::cout<<" distance="<<distance<<" arm_fraction="<<distance/scene::length(axis)<<" mesh="<<best->meshId<<" triangle="<<best->triangleId<<" component="<<best->componentId<<" point="<<match.point.x<<','<<match.point.y<<','<<match.point.z<<" face_normal="<<best->normal.x<<','<<best->normal.y<<','<<best->normal.z<<" normal_dot="<<scene::dot(normal,best->normal)<<" bary="<<match.weights[0]<<','<<match.weights[1]<<','<<match.weights[2]<<" corners=";
                    std::map<uint32_t,float> weights;for(int c=0;c<3;++c){std::cout<<(best->v[c]-native.meshes[best->meshId].vertices.data())<<';';for(int k=0;k<4;++k)if(best->v[c]->weights[k]>0)weights[best->v[c]->bones[k]]+=match.weights[c]*best->v[c]->weights[k];}
                    const auto radial=[&](scene::Vec3 q){const auto d=q-origin;return d-axis*(scene::dot(d,axis)/scene::dot(axis,axis));};
                    const auto rp=radial(p),rq=radial(match.point);
                    std::cout<<" support_axial="<<scene::dot(match.point-origin,axis)/scene::dot(axis,axis)<<" radial_dot="<<scene::dot(scene::normalize(rp),scene::normalize(rq))<<" source_radius="<<scene::length(rp)<<" support_radius="<<scene::length(rq);
                    std::cout<<" weights=";for(const auto& [bone,w]:weights)if(w>0)std::cout<<native.skeleton.bones[bone].name<<':'<<w<<';';std::cout<<'\n';
                }
            }
        }return 0;
    }
    if(argc==7&&std::string_view(argv[1])=="--fit-boundary"){
        const std::string sideText=argv[5];const int side=sideText=="left"?0:sideText=="right"?1:-1;
        float phase{};try{phase=std::stof(argv[6]);}catch(...){return 2;}
        if(side<0||!std::isfinite(phase)||phase<0||phase>1)return 2;
        const auto source=scene::buildScene(cast::Document::load(argv[2]),false);
        auto target=scene::buildScene(cast::Document::load(argv[3]),false);
        scene::appendAnimations(cast::Document::load(argv[4]),target);if(target.animations.empty())return 3;
        auto fitted=target;if(!scene::imported::fitForeignViewSkin(fitted,source,error,true)){std::cerr<<error;return 4;}
        const auto from=scene::imported::viewLayout(source),to=scene::imported::viewLayout(target);
        const auto& arm=from.arms[side];const auto& dest=to.arms[side];if(!arm.valid()||!dest.valid())return 5;
        const auto elbow=scene::transformPoint(source.skeleton.bones[arm.elbow].restGlobal,{});
        const auto axis=scene::transformPoint(source.skeleton.bones[arm.wrist].restGlobal,{})-elbow;
        const auto pose=target.samplePose(0,target.animations[0].durationFrames*phase);
        std::vector<scene::Mat4> skinMatrices;for(size_t b=0;b<pose.size();++b)skinMatrices.push_back(pose[b]*fitted.skeleton.bones[b].inverseBind);
        render::nativeViewhandSkinning(fitted,pose,skinMatrices);
        // Match CrossGameBenchmark::viewCamera, including animated camera bone.
        scene::Vec3 eye{},forward{1,0,0},up{0,0,1};
        const auto camera=target.skeleton.boneByCanonicalName.find("tag_camera");
        if(camera!=target.skeleton.boneByCanonicalName.end()&&camera->second<pose.size()){const auto& c=pose[camera->second];eye={c.v[12],c.v[13],c.v[14]};forward={c.v[0],c.v[1],c.v[2]};up={c.v[8],c.v[9],c.v[10]};}
        const auto vp=scene::perspective(75*scene::kPi/180,960.f/540,.02f,10000)*scene::lookAtDirection(eye,forward,up);
        const auto pixel=[&](scene::Vec3 p){const auto& m=vp.v;const float x=m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],y=m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],w=m[3]*p.x+m[7]*p.y+m[11]*p.z+m[15];return scene::Vec3{(x/w+1)*480,(1-y/w)*270,w};};
        std::cerr<<"camera_eye="<<eye.x<<','<<eye.y<<','<<eye.z<<" forward="<<forward.x<<','<<forward.y<<','<<forward.z<<" up="<<up.x<<','<<up.y<<','<<up.z<<'\n';
        std::cout<<"mesh,a,b,source_incidence,fitted_incidence,source_axial_a,source_axial_b,posed_ax,posed_ay,posed_az,posed_bx,posed_by,posed_bz,pixel_ax,pixel_ay,clip_aw,pixel_bx,pixel_by,clip_bw\n";
        for(const auto& sm:source.meshes)for(const auto& fm:fitted.meshes)if(sm.name==fm.name&&sm.vertices.size()==fm.vertices.size()){
            // Weld only for topology accounting, never modify vertices. This
            // excludes UV/normal seam duplicates from apparent open boundaries.
            std::map<std::tuple<long long,long long,long long>,uint32_t> unique;
            std::vector<uint32_t> canonical(sm.vertices.size());
            for(size_t i=0;i<sm.vertices.size();++i){const auto p=scene::transformPoint(sm.modelTransform,sm.vertices[i].position);
                const auto key=std::make_tuple(std::llround(p.x*100000.),std::llround(p.y*100000.),std::llround(p.z*100000.));
                auto [it,added]=unique.emplace(key,static_cast<uint32_t>(i));canonical[i]=it->second;}
            using Edge=std::pair<uint32_t,uint32_t>;std::map<Edge,int> before,after;std::map<Edge,Edge> representative;
            const auto count=[&](const auto& indices,auto& counts,bool remember){for(size_t t=0;t+2<indices.size();t+=3)for(int k=0;k<3;++k){const auto a=indices[t+k],b=indices[t+(k+1)%3];if(a>=canonical.size()||b>=canonical.size())continue;auto x=canonical[a],y=canonical[b];if(x==y)continue;if(x>y)std::swap(x,y);const Edge edge{x,y};++counts[edge];if(remember)representative[edge]={a,b};}};
            count(sm.indices,before,true);count(fm.indices,after,false);
            // Include the whole requested arm. Omitting shoulder-owned edges
            // makes a proximal boundary look open even when its loop is closed.
            const auto belongs=[&](uint32_t i){const auto& v=fm.vertices[i];for(int k=0;k<4;++k)if(v.weights[k]>0){int b=v.bones[k];for(size_t n=0;b>=0&&n<target.skeleton.bones.size();++n){if(b==dest.shoulder||b==dest.elbow||b==dest.wrist)return true;b=target.skeleton.bones[b].parent;}}return false;};
            const auto animated=[&](uint32_t i){return scene::transformPoint(fm.modelTransform,scene::skinVertexPosition(fm.vertices[i],skinMatrices));};
            const auto axial=[&](uint32_t i){return scene::dot(scene::transformPoint(sm.modelTransform,sm.vertices[i].position)-elbow,axis)/scene::dot(axis,axis);};
            for(const auto& [edge,n]:before){const int kept=after[edge];if(n!=1&&kept==n)continue;const auto [a,b]=representative[edge];if(!belongs(a)&&!belongs(b))continue;const auto p=animated(a),q=animated(b);
                const auto pp=pixel(p),pq=pixel(q);
                std::cout<<'"'<<sm.name<<"\","<<a<<','<<b<<','<<n<<','<<kept<<','<<axial(a)<<','<<axial(b)<<','<<p.x<<','<<p.y<<','<<p.z<<','<<q.x<<','<<q.y<<','<<q.z<<','<<pp.x<<','<<pp.y<<','<<pp.z<<','<<pq.x<<','<<pq.y<<','<<pq.z<<'\n';}
        }return 0;
    }
    if(argc==4&&std::string_view(argv[1])=="--body-clip"){
        auto s=scene::buildScene(cast::Document::load(argv[2]),false);
        scene::appendAnimations(cast::Document::load(argv[3]),s);
        std::cout<<"bones="<<s.skeleton.bones.size()<<" clips="<<s.animations.size()<<'\n';
        for(const auto& a:s.animations){
            std::cout<<a.sourceName<<" tracks="<<a.tracks.size()<<" source="<<a.sourceCurveCount<<" unmapped="<<a.unmappedCurveCount<<" duration="<<a.durationFrames<<'\n';
            std::set<std::size_t> bound;for(const auto& t:a.tracks)bound.insert(t.boneIndex);
            for(const auto b:bound)if(b<s.skeleton.bones.size()){
                float scalarSpan=0,rotationDelta=0;
                for(const auto& t:a.tracks)if(t.boneIndex==b){
                    if(!t.scalarValues.empty()){auto range=std::minmax_element(t.scalarValues.begin(),t.scalarValues.end());scalarSpan=std::max(scalarSpan,*range.second-*range.first);}
                    if(!t.rotationValues.empty()){const auto q=t.rotationValues.front();for(const auto& r:t.rotationValues){
                        const float same=std::abs(q.x-r.x)+std::abs(q.y-r.y)+std::abs(q.z-r.z)+std::abs(q.w-r.w);
                        const float opposite=std::abs(q.x+r.x)+std::abs(q.y+r.y)+std::abs(q.z+r.z)+std::abs(q.w+r.w);
                        rotationDelta=std::max(rotationDelta,std::min(same,opposite));}}
                }
                std::cout<<"bound="<<s.skeleton.bones[b].name<<" scalar_span="<<scalarSpan<<" rotation_component_delta="<<rotationDelta<<'\n';
            }
        }
        return s.animations.empty()?3:0;
    }
    if(argc==4&&std::string_view(argv[1])=="--arm-edges"){
        const auto source=scene::buildScene(cast::Document::load(argv[2]),false);
        auto fitted=scene::buildScene(cast::Document::load(argv[3]),false);
        if(!scene::imported::fitForeignViewSkin(fitted,source,error)){std::cerr<<error;return 3;}
        const auto layout=scene::imported::viewLayout(fitted);
        for(int side=0;side<2;++side){const auto& arm=layout.arms[side];if(!arm.valid()||arm.shoulder<0)continue;
            const auto origin=scene::transformPoint(fitted.skeleton.bones[arm.shoulder].restGlobal,{});
            const auto delta=scene::transformPoint(fitted.skeleton.bones[arm.elbow].restGlobal,{})-origin;
            const float length=scene::length(delta);const auto axis=delta/length;
            for(const auto& mesh:fitted.meshes){if(mesh.viewmodelWeapon)continue;
                struct Edge{uint32_t a,b;float span,x,y,wa,wb;};std::vector<Edge> edges;
                std::set<std::pair<uint32_t,uint32_t>> seen;
                const auto weight=[&](uint32_t i){float w=0;const auto& v=mesh.vertices.at(i);for(int k=0;k<4;++k)if(v.bones[k]==static_cast<uint32_t>(arm.shoulder))w+=v.weights[k];return w;};
                const auto along=[&](uint32_t i){return scene::dot(scene::transformPoint(mesh.modelTransform,mesh.vertices.at(i).position)-origin,axis)/length;};
                for(size_t t=0;t+2<mesh.indices.size();t+=3)for(int k=0;k<3;++k){auto a=mesh.indices[t+k],b=mesh.indices[t+(k+1)%3];if(a>b)std::swap(a,b);if(!seen.emplace(a,b).second)continue;
                    const float wa=weight(a),wb=weight(b);if(wa<=.01f&&wb<=.01f)continue;
                    const float x=along(a),y=along(b);if(std::min(x,y)>=.7f||std::max(x,y)<=.7f)continue;
                    edges.push_back({a,b,std::abs(y-x),x,y,wa,wb});}
                std::sort(edges.begin(),edges.end(),[](const auto& a,const auto& b){return a.span>b.span;});
                std::cout<<"side="<<side<<" mesh="<<mesh.name<<" crossing_edges="<<edges.size()<<'\n';
                for(size_t i=0;i<std::min<size_t>(8,edges.size());++i){const auto& e=edges[i];std::cout<<e.a<<','<<e.b<<",axial="<<e.x<<','<<e.y<<",shoulder_weights="<<e.wa<<','<<e.wb<<'\n';}
            }
        }return 0;
    }
    // Old five-argument edge probe remains compatible. Explicit anatomy and
    // vertex selection avoids confusing the opposite arm's maximum edge with
    // the actual visible defect. Vertex mode emits all vertices for a side so
    // a renderer-space ROI can be traced back without selecting by stretch.
    const bool fitEdgeProbe=argc>=2&&std::string_view(argv[1])=="--fit-edge-paths";
    const bool fitVertexProbe=argc>=2&&std::string_view(argv[1])=="--fit-vertices";
    if((fitEdgeProbe&&(argc==5||argc==8||argc==9))||(fitVertexProbe&&(argc==6||argc==7))){
        int side=1;std::array<std::size_t,2> selected{3311u,3318u};float phase=.55f;
        if(argc>5){const std::string sideText=argv[5];if(sideText=="left"||sideText=="0")side=0;else if(sideText=="right"||sideText=="1")side=1;else{std::cerr<<"Side must be left or right\n";return 2;}}
        try{
            if(fitEdgeProbe&&argc>=8){selected={std::stoul(argv[6]),std::stoul(argv[7])};if(argc==9)phase=std::stof(argv[8]);}
            if(fitVertexProbe&&argc==7)phase=std::stof(argv[6]);
        }catch(const std::exception&){std::cerr<<"Invalid vertex index or phase\n";return 2;}
        if(!std::isfinite(phase)||phase<0||phase>1){std::cerr<<"Phase must be in [0,1]\n";return 2;}
        const auto source=scene::buildScene(cast::Document::load(argv[2]),false);
        auto target=scene::buildScene(cast::Document::load(argv[3]),false);
        scene::appendAnimations(cast::Document::load(argv[4]),target);
        if(target.animations.empty())return 3;
        const auto from=scene::imported::viewLayout(source),to=scene::imported::viewLayout(target);
        auto fitted=target;if(!scene::imported::fitForeignViewSkin(fitted,source,error,true)){std::cerr<<error;return 4;}
        const auto pose=target.samplePose(0,target.animations[0].durationFrames*phase);
        const auto pos=[](const scene::Skeleton& s,int b){return scene::transformPoint(s.bones.at(b).restGlobal,{});};
        const auto& a=from.arms[side];const auto& b=to.arms[side];
        if(!a.valid()||!b.valid()||a.shoulder<0||b.shoulder<0){std::cerr<<"Requested arm is incomplete\n";return 5;}
        if(fitVertexProbe){
            std::vector<scene::Mat4> skinMatrices;for(size_t bone=0;bone<pose.size();++bone)skinMatrices.push_back(pose[bone]*fitted.skeleton.bones[bone].inverseBind);
            render::nativeViewhandSkinning(fitted,pose,skinMatrices);
            std::cout<<"mesh,vertex,side,phase,source_x,source_y,source_z,fitted_x,fitted_y,fitted_z,posed_x,posed_y,posed_z,source_weights,fitted_weights\n";
            const auto weights=[](const scene::Skeleton& skeleton,const auto& vertex){std::string text;for(int k=0;k<4;++k)if(vertex.weights[k]>0)text+=skeleton.bones.at(vertex.bones[k]).name+":"+std::to_string(vertex.weights[k])+";";return text;};
            for(const auto& sm:source.meshes)for(const auto& fm:fitted.meshes)if(fm.name==sm.name&&fm.vertices.size()==sm.vertices.size()){
                for(std::size_t i=0;i<fm.vertices.size();++i){const auto& sv=sm.vertices[i];const auto& fv=fm.vertices[i];bool belongs=false;
                    for(int k=0;k<4;++k)if(fv.weights[k]>0){int bone=static_cast<int>(fv.bones[k]);for(std::size_t n=0;bone>=0&&n<target.skeleton.bones.size();++n){if(bone==b.shoulder||bone==b.elbow||bone==b.wrist){belongs=true;break;}bone=target.skeleton.bones.at(bone).parent;}}
                    if(!belongs)continue;
                    const auto original=scene::transformPoint(sm.modelTransform,sv.position),bind=scene::transformPoint(fm.modelTransform,fv.position);
                    const auto animated=scene::transformPoint(fm.modelTransform,scene::skinVertexPosition(fv,skinMatrices));
                    std::cout<<'"'<<sm.name<<"\","<<i<<','<<side<<','<<phase<<','<<original.x<<','<<original.y<<','<<original.z<<','<<bind.x<<','<<bind.y<<','<<bind.z<<','<<animated.x<<','<<animated.y<<','<<animated.z<<",\""<<weights(source.skeleton,sv)<<"\",\""<<weights(target.skeleton,fv)<<"\"\n";
                }
            }return 0;
        }
        const auto sn=scene::normalize(scene::cross(pos(source.skeleton,a.fingers[1][0])-pos(source.skeleton,a.wrist),pos(source.skeleton,a.fingers[4][0])-pos(source.skeleton,a.wrist)));
        const auto tn=scene::normalize(scene::cross(pos(target.skeleton,b.fingers[1][0])-pos(target.skeleton,b.wrist),pos(target.skeleton,b.fingers[4][0])-pos(target.skeleton,b.wrist)));
        const auto transform=[&](int sb,int se,int tb,int te){const auto sp=pos(source.skeleton,sb),tp=pos(target.skeleton,tb);const float scale=scene::length(pos(target.skeleton,te)-tp)/scene::length(pos(source.skeleton,se)-sp);
            return scene::imported::viewAnatomicalFrame(tp,pos(target.skeleton,te),tn)*scene::scale({scale,scale,scale})*scene::inverseAffine(scene::imported::viewAnatomicalFrame(sp,pos(source.skeleton,se),sn));};
        const auto upper=transform(a.shoulder,a.elbow,b.shoulder,b.elbow),lower=transform(a.elbow,a.wrist,b.elbow,b.wrist);
        const auto rotationGap=[](const scene::Mat4& x,const scene::Mat4& y){scene::Vec3 p,s;scene::Quat q,r;scene::decomposeAffine(x,p,q,s);scene::decomposeAffine(y,p,r,s);q=scene::normalize(q);r=scene::normalize(r);return 2*std::acos(std::clamp(std::abs(q.x*r.x+q.y*r.y+q.z*r.z+q.w*r.w),0.f,1.f))*180/scene::kPi;};
        std::cout<<"fit_rotation_gap_degrees="<<rotationGap(upper,lower)<<" pose_skin_rotation_gap_degrees="<<rotationGap(pose[b.shoulder]*target.skeleton.bones[b.shoulder].inverseBind,pose[b.elbow]*target.skeleton.bones[b.elbow].inverseBind)<<'\n';
        for(const auto& sm:source.meshes)if(sm.name.ends_with("section_0"))for(const auto& fm:fitted.meshes)if(fm.name==sm.name){
            std::array<scene::Vec3,2> original{},baked{},direct{},directBind{};
            if(selected[0]>=sm.vertices.size()||selected[1]>=sm.vertices.size()||selected[0]>=fm.vertices.size()||selected[1]>=fm.vertices.size()){std::cerr<<"Selected vertex is outside section_0\n";return 2;}
            for(int n=0;n<2;++n){const auto i=selected[n];const auto& sv=sm.vertices.at(i);const auto& fv=fm.vertices.at(i);original[n]=scene::transformPoint(sm.modelTransform,sv.position);
                std::cout<<"vertex="<<i<<" separate_fit_points_gap="<<scene::length(scene::transformPoint(upper,original[n])-scene::transformPoint(lower,original[n]))<<'\n';
                for(int k=0;k<4;++k)if(sv.weights[k]>0){int bone=sv.bones[k];while(bone>=0&&bone!=a.shoulder&&bone!=a.elbow)bone=source.skeleton.bones[bone].parent;if(bone<0)return 5;
                    const auto dest=bone==a.shoulder?b.shoulder:b.elbow;const auto bind=scene::transformPoint(bone==a.shoulder?upper:lower,original[n]);
                    directBind[n]+=bind*sv.weights[k];direct[n]+=scene::transformPoint(pose[dest]*target.skeleton.bones[dest].inverseBind,bind)*sv.weights[k];
                }
                for(int k=0;k<4;++k)if(fv.weights[k]>0)baked[n]+=scene::transformPoint(pose[fv.bones[k]]*target.skeleton.bones[fv.bones[k]].inverseBind,fv.position)*fv.weights[k];
            }
            std::cout<<"source="<<scene::length(original[1]-original[0])<<" baked_pose="<<scene::length(baked[1]-baked[0])<<" direct_bind="<<scene::length(directBind[1]-directBind[0])<<" direct_pose="<<scene::length(direct[1]-direct[0])<<'\n';
        }return 0;
    }
    if(argc==4&&std::string_view(argv[1])=="--view-fit-arms"){
        const auto skin=scene::buildScene(cast::Document::load(argv[2]),false);
        const auto native=scene::buildScene(cast::Document::load(argv[3]),false);
        auto fitted=native;
        if(!scene::imported::fitForeignViewSkin(fitted,skin,error)){std::cerr<<error<<'\n';return 1;}
        const auto layout=scene::imported::viewLayout(native);
        const auto pos=[&](int b){return scene::transformPoint(native.skeleton.bones.at(b).restGlobal,{});};
        std::cout<<"model,side,segment,axis,count,min,p10,median,p90,max\n";
        for(int side=0;side<2;++side){const auto& arm=layout.arms[side];
            if(!arm.valid()||arm.shoulder<0)continue;
            const auto normal=scene::normalize(scene::cross(pos(arm.fingers[1][0])-pos(arm.wrist),pos(arm.fingers[4][0])-pos(arm.wrist)));
            for(int segment=0;segment<2;++segment){const int bone=segment?arm.elbow:arm.shoulder,next=segment?arm.wrist:arm.elbow;
                const auto frame=scene::inverseAffine(scene::imported::viewAnatomicalFrame(pos(bone),pos(next),normal));
                for(int model=0;model<2;++model){std::array<std::vector<float>,3> samples;
                    for(const auto& mesh:(model?fitted:native).meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices){
                        float weight=0;for(int k=0;k<4;++k)if(v.bones[k]==static_cast<uint32_t>(bone))weight+=v.weights[k];
                        if(weight<.5f)continue;
                        const auto p=scene::transformPoint(frame,scene::transformPoint(mesh.modelTransform,v.position));
                        samples[0].push_back(p.x);samples[1].push_back(p.y);samples[2].push_back(p.z);
                    }
                    for(int axis=0;axis<3;++axis){auto& values=samples[axis];if(values.empty())continue;std::sort(values.begin(),values.end());const auto last=values.size()-1;
                        std::cout<<(model?"fitted":"native")<<','<<side<<','<<segment<<','<<axis<<','<<values.size()<<','<<values.front()<<','<<values[last/10]<<','<<values[last/2]<<','<<values[last*9/10]<<','<<values.back()<<'\n';}
                }
            }
        }return 0;
    }
    if(argc==4&&std::string_view(argv[1])=="--view-fit-fingers"){
        const auto skin=scene::buildScene(cast::Document::load(argv[2]),false);
        const auto native=scene::buildScene(cast::Document::load(argv[3]),false);
        auto fitted=native;
        if(!scene::imported::fitForeignViewSkin(fitted,skin,error)){std::cerr<<error<<'\n';return 1;}
        const auto layout=scene::imported::viewLayout(native);
        // Compare high-influence skin centroids in the same native bind space.
        // These are geometric diagnostics, not visual/contact pass criteria.
        const auto centroid=[](const scene::CastScene& model,int bone){
            scene::Vec3 sum{};float total{};
            for(const auto& mesh:model.meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices){
                float w{};for(int k=0;k<4;++k)if(v.bones[k]==static_cast<unsigned>(bone))w+=v.weights[k];
                if(w>=.75f){sum+=scene::transformPoint(mesh.modelTransform,v.position)*w;total+=w;}
            }
            return std::pair{total,total>0?sum/total:scene::Vec3{}};
        };
        std::cout<<"side,digit,joint,bone,native_weight,fitted_weight,centroid_gap,native_axis_length,fitted_axis_length,axis_dot\n";
        for(int side=0;side<2;++side)for(int f=0;f<5;++f)for(int j=0;j<3;++j){
            const int b=layout.arms[side].fingers[f][j];if(b<0)continue;
            const auto [nw,np]=centroid(native,b);const auto [fw,fp]=centroid(fitted,b);
            const auto origin=scene::transformPoint(native.skeleton.bones[b].restGlobal,{});
            const auto na=np-origin,fa=fp-origin;
            std::cout<<side<<','<<f<<','<<j<<','<<native.skeleton.bones[b].name<<','<<nw<<','<<fw<<','<<scene::length(np-fp)<<','<<scene::length(na)<<','<<scene::length(fa)<<','<<scene::dot(scene::normalize(na),scene::normalize(fa))<<'\n';
        }return 0;
    }
    if((argc==7||argc==8)&&std::string_view(argv[1])=="--t9-iw-edges"){
        auto target=scene::buildScene(cast::Document::load(argv[2]));
        auto source=scene::buildScene(cast::Document::load(argv[3]));
        const auto weapon=cast::Document::load(argv[4]);
        scene::appendRigModel(weapon,target,"weapon");scene::appendRigModel(weapon,source,"weapon");
        const auto clip=cast::Document::load(argv[5]);
        scene::appendAnimations(clip,target);scene::appendAnimations(clip,source);
        if(!scene::retargetColdWarLegacyRange(target,0,source,0))return 3;
        // Diagnostic-only candidate. Do not change production skin until the
        // full render and mixed-material/connected-component cases are tested.
        if(argc==8&&std::string_view(argv[7])=="sleeve-bind"){
            const auto spine=target.skeleton.boneByName.at("j_spineupper");
            const auto left=target.skeleton.boneByName.at("j_clavicle_le");
            const auto right=target.skeleton.boneByName.at("j_clavicle_ri");
            for(auto& mesh:target.meshes){bool shoulder=false,body=false;
                for(const auto& v:mesh.vertices)for(int k=0;k<4;++k)if(v.weights[k]>0){
                    const auto& name=target.skeleton.bones[v.bones[k]].name;
                    shoulder=shoulder||name.starts_with("j_shouldertwist");
                    body=body||name.starts_with("j_hip")||name.starts_with("j_knee");
                }
                if(!shoulder||body||mesh.viewmodelWeapon)continue;
                for(auto& v:mesh.vertices){
                    const auto lp=scene::transformPoint(target.skeleton.bones[left].restGlobal,{});
                    const auto rp=scene::transformPoint(target.skeleton.bones[right].restGlobal,{});
                    const auto side=scene::length(v.position-lp)<scene::length(v.position-rp)?left:right;
                    for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]==spine)v.bones[k]=side;
                }
            }
        }
        const auto pose=target.samplePose(0,target.animations[0].durationFrames*std::stof(argv[6]));
        for(const auto* name:{"tag_view","tag_torso","j_spineupper","j_clavicle_le","j_shoulder_le","j_shouldertwist_le","j_clavicle_ri","j_shoulder_ri"}){
            const auto it=target.skeleton.boneByName.find(name);if(it==target.skeleton.boneByName.end())continue;
            const auto p=scene::transformPoint(pose[it->second],{});
            std::cout<<"POSE "<<name<<' '<<p.x<<','<<p.y<<','<<p.z<<'\n';
        }
        for(const auto& mesh:target.meshes){
            if(mesh.viewmodelWeapon)continue;
            std::vector<scene::Vec3> deformed;
            for(const auto& v:mesh.vertices){scene::Vec3 p{};
                for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<pose.size())
                    p=p+scene::transformPoint(pose[v.bones[k]]*target.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];
                deformed.push_back(p);
            }
            struct Edge{float ratio;unsigned a,b;};std::vector<Edge> edges;
            for(std::size_t t=0;t+2<mesh.indices.size();t+=3)for(int k=0;k<3;++k){
                const auto a=mesh.indices[t+k],b=mesh.indices[t+(k+1)%3];
                const float rest=scene::length(mesh.vertices[a].position-mesh.vertices[b].position);
                if(rest>.01f)edges.push_back({scene::length(deformed[a]-deformed[b])/rest,a,b});
            }
            std::sort(edges.begin(),edges.end(),[](const auto& a,const auto& b){return a.ratio>b.ratio;});
            std::cout<<"MESH "<<mesh.name<<" material="<<mesh.materialName<<'\n';
            for(std::size_t i=0;i<std::min<std::size_t>(3,edges.size());++i){const auto& e=edges[i];std::cout<<"ratio="<<e.ratio;
                for(auto vi:{e.a,e.b}){std::cout<<" vertex="<<vi;const auto& v=mesh.vertices[vi];
                    for(int k=0;k<4;++k)if(v.weights[k]>0)std::cout<<' '<<target.skeleton.bones[v.bones[k]].name<<':'<<v.weights[k];
                }std::cout<<'\n';
            }
        }return 0;
    }
    if(argc==5&&std::string_view(argv[1])=="--clip-clock"){
        scene::CastScene s;
        if(!scene::imported::assemble(cast::Document::load(argv[2]),cast::Document::load(argv[3]),s,error)){std::cerr<<error;return 2;}
        scene::appendAnimations(cast::Document::load(argv[4]),s);
        for(const auto& a:s.animations)std::cout<<a.name<<" frames="<<a.durationFrames<<" fps="<<a.framerate<<" seconds="<<(a.framerate>0?a.durationFrames/a.framerate:0)<<" tracks="<<a.tracks.size()<<'\n';
        return s.animations.empty()?3:0;
    }
    if(argc==3&&std::string_view(argv[1])=="--body-fingers"){
        const auto s=scene::buildScene(cast::Document::load(argv[2]),false);
        const auto chains=scene::imported::bodyFingerChains(s.skeleton);
        for(int side=0;side<2;++side){std::cout<<"side="<<side<<" palm="<<bool(scene::imported::bodyPalmRestFrame(s.skeleton,side))<<'\n';
            for(int digit=0;digit<5;++digit){std::cout<<"digit="<<digit;for(int joint:chains[side][digit])std::cout<<','<<(joint<0?"MISSING":s.skeleton.bones[joint].name);std::cout<<'\n';}
        }return 0;
    }
    if((argc==5||argc==6)&&std::string_view(argv[1])=="--codm-roll"){
        scene::CastScene s;
        if(!scene::codm::assemble(cast::Document::load(argv[2]),cast::Document::load(argv[3]),s,error)){std::cerr<<error;return 2;}
        scene::appendAnimations(cast::Document::load(argv[4]),s);if(s.animations.empty())return 3;
        for(float fraction:{0.f,.5f,.95f}){const auto pose=s.samplePose(0,s.animations[0].durationFrames*fraction);
            for(const auto* side:{"Left","Right"})for(const auto* part:{"ForeArm","ForeArmRoll","Hand"}){
                const auto name="b_"+std::string(side)+part;const auto i=s.skeleton.boneByName.at(name);
                const auto delta=pose[i]*s.skeleton.bones[i].inverseBind;
                std::cout<<fraction<<','<<name<<",skin_delta=";for(float v:delta.v)std::cout<<v<<',';std::cout<<'\n';
            }
        }
        if(argc==6){auto fitted=s;if(!scene::imported::fitForeignViewSkin(fitted,scene::buildScene(cast::Document::load(argv[5])),error)){std::cerr<<error;return 4;}
            for(const auto* side:{"Left","Right"}){const auto name="b_"+std::string(side)+"ForeArmRoll";const auto roll=s.skeleton.boneByName.at(name);double nativeWeight{},fittedWeight{};
                for(const auto& m:s.meshes)if(!m.viewmodelWeapon)for(const auto& v:m.vertices)for(int k=0;k<4;++k)if(v.weights[k]>0){auto id=v.bones[k];for(const auto& [follower,driver]:s.nativePoseFollowers)if(id==follower){id=driver;break;}if(id==roll)nativeWeight+=v.weights[k];}
                for(const auto& m:fitted.meshes)if(!m.viewmodelWeapon)for(const auto& v:m.vertices)for(int k=0;k<4;++k)if(v.bones[k]==roll)fittedWeight+=v.weights[k];
                std::cout<<name<<",native_weight="<<nativeWeight<<",fitted_weight="<<fittedWeight<<'\n';
            }
        }return 0;
    }
    if(argc==3&&std::string_view(argv[1])=="--materials"){
        const auto doc=cast::Document::load(argv[2]);
        const auto visit=[&](auto&& self,const cast::Node& n)->void{
            if(n.identifier==0x6c74616d||n.identifier==0x656c6966){std::cout<<"node="<<n.identifier<<'\n';for(const auto& p:n.properties)std::cout<<p.name<<'='<<doc.propertyPreview(p,8)<<'\n';}
            for(const auto& child:n.children)self(self,child);
        };for(const auto& root:doc.roots())visit(visit,root);return 0;
    }
    if(argc==3&&std::string_view(argv[1])=="--skeleton"){
        const auto s=scene::buildScene(cast::Document::load(argv[2]),false);
        for(std::size_t i=0;i<s.skeleton.bones.size();++i){const auto& b=s.skeleton.bones[i];const auto p=scene::transformPoint(b.restGlobal,{});std::cout<<i<<','<<b.name<<','<<b.parent<<','<<p.x<<','<<p.y<<','<<p.z<<'\n';}return 0;
    }
    if(argc==3&&std::string_view(argv[1])=="--geometry"){
        const auto s=scene::buildScene(cast::Document::load(argv[2]),false);
        std::cout<<"bones="<<s.skeleton.bones.size()<<" meshes="<<s.meshes.size()<<'\n';
        for(const auto& m:s.meshes){std::set<std::uint32_t> weighted;
            for(const auto& v:m.vertices)for(int k=0;k<4;++k)if(v.weights[k]>0)weighted.insert(v.bones[k]);
            std::cout<<m.name<<" vertices="<<m.vertices.size()<<" triangles="<<m.indices.size()/3<<" material="<<m.materialName<<" albedo="<<m.albedoPath<<" bones=";
            for(auto i:weighted)if(i<s.skeleton.bones.size())std::cout<<s.skeleton.bones[i].name<<';';std::cout<<'\n';
            std::cout<<" color="<<m.color.x<<','<<m.color.y<<','<<m.color.z<<','<<m.color.w<<" camo="<<m.camoBlend<<" alpha="<<m.camoUseAlpha<<" vertexColor="<<m.useVertexColor<<" metal="<<m.metallicFactor<<" uv=";
            if(!m.vertices.empty())std::cout<<m.vertices.front().uv.x<<','<<m.vertices.front().uv.y;
            std::cout<<'\n';
        }return 0;
    }
    if(argc==3&&std::string_view(argv[1])=="--metadata"){
        const auto doc=cast::Document::load(argv[2]);
        const auto visit=[&](auto&& self,const cast::Node& n,int depth)->void{
            if(depth<2)for(const auto& p:n.properties)std::cout<<p.name<<'='<<doc.propertyPreview(p,8)<<'\n';
            for(const auto& c:n.children)self(self,c,depth+1);
        };for(const auto& n:doc.roots())visit(visit,n,0);return 0;
    }
    if(argc==3&&std::string_view(argv[1])=="--muzzle"){
        const auto s=scene::buildScene(cast::Document::load(argv[2]),false);
        for(std::size_t i=0;i<s.skeleton.bones.size();++i){const auto& b=s.skeleton.bones[i];std::cout<<i<<' '<<b.name<<" parent="<<b.parent<<" pos="<<b.restGlobal.v[12]<<','<<b.restGlobal.v[13]<<','<<b.restGlobal.v[14]<<'\n';}
        for(const auto& a:s.muzzleAnchors){std::cout<<"anchor parent="<<a.bone<<" local="<<a.local.x<<','<<a.local.y<<','<<a.local.z<<'\n';}
        return 0;
    }
    if(argc==3&&std::string_view(argv[1])=="--db-index"){
        for(const auto& entry:std::filesystem::directory_iterator(argv[2]))if(entry.path().extension()==".cast"){
            const auto doc=cast::Document::load(entry.path());std::string source;std::set<std::string> bones;
            const auto visit=[&](auto&& self,const cast::Node& n)->void{if(auto p=n.findProperty("db.source");p&&p->stringValue)source=*p->stringValue;if(auto p=n.findProperty("nn");p&&p->stringValue)bones.insert(*p->stringValue);for(const auto& c:n.children)self(self,c);};for(const auto& r:doc.roots())visit(visit,r);
            std::cout<<entry.path().filename().string()<<'\t'<<source<<'\t'<<bones.size()<<'\t'<<bones.contains("SkelBone_R_Hand")<<'\n';
        }return 0;
    }
    if(argc==5&&std::string_view(argv[1])=="--pose"){
        scene::CastScene s;if(!scene::imported::assemble(cast::Document::load(argv[2]),cast::Document::load(argv[3]),s,error)){std::cerr<<error;return 2;}scene::appendAnimations(cast::Document::load(argv[4]),s);if(s.animations.empty())return 3;
        auto pose=s.samplePose(0,0);for(std::size_t i=0;i<s.skeleton.bones.size();++i){const auto& n=s.skeleton.bones[i].name;if(n.find("Root")!=std::string::npos||n.find("Hand")!=std::string::npos||n.find("tag_")!=std::string::npos||n.find("Muzzle")!=std::string::npos)std::cout<<n<<" pos="<<pose[i].v[12]<<','<<pose[i].v[13]<<','<<pose[i].v[14]<<'\n';}return 0;
    }
    if(argc>2&&std::string_view(argv[1])=="--curves"){const auto doc=cast::Document::load(argv[2]);std::set<std::string> names;const auto visit=[&](auto&& self,const cast::Node& n)->void{if(const auto* p=n.findProperty("nn");p&&p->stringValue){std::string line=*p->stringValue;for(const auto key:{"kp","m","kv"})if(const auto* field=n.findProperty(key))line+=" "+std::string(key)+"="+doc.propertyPreview(*field,4);names.insert(line);}for(const auto& c:n.children)self(self,c);};for(const auto& r:doc.roots())visit(visit,r);for(const auto& n:names)std::cout<<n<<'\n';return 0;}
    for(const auto game:{"gitsfa","dirtybomb","blacksquad"}){
        assets::Catalog catalog;const auto root=std::filesystem::path(argv[1])/game;
        if(!assets::appendScan(root,game,catalog,error)){std::cerr<<error;return 2;}
        std::vector<std::filesystem::path> clips;for(const auto& f:std::filesystem::directory_iterator(root/"animations"))if(f.path().extension()==".cast")clips.push_back(f.path());
        for(const auto& a:catalog.entries){
            auto s=scene::buildScene(cast::Document::load(a.path),false);std::cout<<game<<' '<<assets::roleName(a.role)<<' '<<a.name<<" category="<<a.category;
            if(a.role==assets::Role::ViewHands)std::cout<<" anatomy="<<scene::imported::viewLayout(s).valid();
            if(a.role==assets::Role::PlayerModel)std::cout<<" anatomy="<<scene::imported::completeBodyLayout(scene::imported::bodyOrCodLayout(s.skeleton));
            if(a.role==assets::Role::ViewWeapon){std::size_t count{},curves{},missing{};for(const auto& clip:clips)if(assets::imported::matchesAnimation(game,a.name,clip.filename().string())){++count;if(count==1){scene::appendAnimations(cast::Document::load(clip),s);for(const auto& anim:s.animations){curves+=anim.sourceCurveCount;missing+=anim.unmappedCurveCount;}}}std::cout<<" clips="<<count<<" first_curves="<<curves<<" missing="<<missing;}
            std::cout<<'\n';
        }
    }
}
