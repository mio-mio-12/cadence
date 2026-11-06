#include "scene/ImportedViewRetarget.h"
#include "scene/PointBlankAdapter.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
int main(int argc,char** argv){try{
    if(argc<6)return 2;std::string error;
    const auto wd=cast::Document::load(argv[1]),hd=cast::Document::load(argv[2]),sd=cast::Document::load(argv[3]),ad=cast::Document::load(argv[4]);
    if(!wd.valid()||!hd.valid()||!sd.valid()||!ad.valid()){std::cerr<<"Invalid input\n";return 2;}
    auto native=scene::buildScene(hd,false);const auto skin=scene::buildScene(sd,false);
    for(const auto& b:skin.skeleton.bones)if(b.name.find("_meta_L")!=std::string::npos||b.name=="hand_L"||b.name=="arm_lower_L_TWIST1"||b.name=="arm_lower_L"||b.name=="finger_index_0_L"){auto p=scene::transformPoint(b.restGlobal,{});std::cout<<"META "<<b.name<<" "<<p.x<<","<<p.y<<","<<p.z<<'\n';}
    for(const auto& b:skin.skeleton.bones)if(b.name.find("Twist")!=std::string::npos){const auto p=scene::transformPoint(b.restGlobal,{});std::cout<<"TWIST "<<b.name<<" "<<p.x<<","<<p.y<<","<<p.z<<'\n';}
    for(std::size_t i=0;i<skin.skeleton.bones.size();++i){float weight{};for(const auto& m:skin.meshes)for(const auto& v:m.vertices)for(int k=0;k<4;++k)if(v.bones[k]==i)weight+=v.weights[k];if(weight>0)std::cout<<"SKIN_WEIGHT "<<i<<' '<<skin.skeleton.bones[i].name<<" parent="<<skin.skeleton.bones[i].parent<<" weight="<<weight<<'\n';}
    {const auto layout=scene::imported::viewLayout(skin);if(layout.valid())for(int side=0;side<2;++side){std::cout<<"SKIN_DIGITS "<<side;for(const auto& digit:layout.arms[side].fingers)std::cout<<' '<<(digit[0]>=0?skin.skeleton.bones[digit[0]].name:"(absent)");std::cout<<'\n';}}
    if(scene::pointblank::exportedByPb2cast(wd)){if(!scene::pointblank::assemble(wd,hd,native,error)){std::cerr<<error;return 2;}}
    else if(assets::imported::supported(scene::imported::gameForPath(argv[1]))){scene::CastScene merged;if(!scene::imported::assemblePrepared(scene::buildScene(wd,false),std::move(native),merged,error)){std::cerr<<error;return 2;}native=std::move(merged);}
    else if(!scene::appendRigModel(wd,native,"native weapon")){std::cerr<<"Cannot mount native weapon\n";return 2;}
    if(!scene::appendAnimations(ad,native)){std::cerr<<"No native clip\n";return 2;}
    {const auto a=scene::imported::viewLayout(skin),b=scene::imported::viewLayout(native);if(a.valid()&&b.valid())for(int side=0;side<2;++side){const auto pos=[](const auto& s,int bone){return scene::transformPoint(s.skeleton.bones[bone].restGlobal,{});};const auto& x=a.arms[side];const auto& y=b.arms[side];std::cout<<"anatomy side="<<side<<" palm_scale="<<scene::length(pos(native,y.fingers[1][0])-pos(native,y.fingers[4][0]))/scene::length(pos(skin,x.fingers[1][0])-pos(skin,x.fingers[4][0]))<<" arm_scale="<<scene::length(pos(native,y.wrist)-pos(native,y.elbow))/scene::length(pos(skin,x.wrist)-pos(skin,x.elbow))<<'\n';}}
    for(const auto* model:std::array<const scene::CastScene*,2>{&skin,&native}){const auto layout=scene::imported::viewLayout(*model);for(int side=0;side<2;++side){std::cout<<(model==&skin?"donor":"driver")<<" side="<<side;for(int b:{layout.arms[side].shoulder,layout.arms[side].elbow,layout.arms[side].wrist})if(b>=0){const auto& bone=model->skeleton.bones[b];const auto p=scene::transformPoint(bone.restGlobal,{});std::cout<<' '<<bone.name<<'('<<p.x<<','<<p.y<<','<<p.z<<')';}std::cout<<'\n';}}
    {const auto layout=scene::imported::viewLayout(skin);for(const auto& arm:layout.arms)if(arm.shoulder>=0){const auto e=scene::transformPoint(skin.skeleton.bones[arm.elbow].restGlobal,{}),s=scene::transformPoint(skin.skeleton.bones[arm.shoulder].restGlobal,{});std::map<std::string,float> counts;for(const auto& m:skin.meshes)for(const auto& v:m.vertices){const auto p=scene::transformPoint(m.modelTransform,v.position);if(scene::length(p-s)<scene::length(p-e))for(int k=0;k<4;++k)if(v.weights[k]>0)counts[skin.skeleton.bones[v.bones[k]].name]+=v.weights[k];}for(const auto& [name,weight]:counts)std::cout<<"UPPER "<<name<<' '<<weight<<'\n';}}
    {const auto layout=scene::imported::viewLayout(skin);for(const auto& arm:layout.arms)for(int b:{arm.shoulder,arm.elbow})if(b>=0){scene::Vec3 center{};float count{};for(const auto& m:skin.meshes)for(const auto& v:m.vertices)for(int k=0;k<4;++k)if(v.bones[k]==b&&v.weights[k]>.5f){center+=scene::transformPoint(m.modelTransform,v.position);++count;}if(count)center=center/count;std::cout<<"CENTER "<<skin.skeleton.bones[b].name<<' '<<center.x<<','<<center.y<<','<<center.z<<' '<<count<<'\n';}}
    std::vector<std::vector<scene::Mat4>> reference;for(int i=0;i<21;++i)reference.push_back(native.samplePose(0,native.animations[0].durationFrames*i/20.f));
    {const auto layout=scene::imported::viewLayout(native);const auto pose=native.samplePose(0,0);for(const auto& arm:layout.arms){const auto e=scene::transformPoint(pose[arm.elbow],{}),w=scene::transformPoint(pose[arm.wrist],{}),predicted=scene::transformPoint(pose[arm.elbow]*native.skeleton.bones[arm.elbow].inverseBind,scene::transformPoint(native.skeleton.bones[arm.wrist].restGlobal,{}));std::cout<<"POSE elbow "<<e.x<<','<<e.y<<','<<e.z<<" wrist "<<w.x<<','<<w.y<<','<<w.z<<" predicted "<<predicted.x<<','<<predicted.y<<','<<predicted.z<<'\n';}}
    const auto baseline=native;const auto bones=native.skeleton.bones.size();if(!(argc>6?scene::imported::fitPoseOnlyViewSkin(native,skin,error):scene::imported::fitForeignViewSkin(native,skin,error))){std::cerr<<error<<'\n';return 2;}
    {const auto layout=scene::imported::viewLayout(skin);const auto target=scene::imported::viewLayout(native);for(int side=0;side<2;++side){scene::Vec3 center{};float n{};const auto& a=layout.arms[side];for(const auto& sm:skin.meshes)for(const auto& dm:native.meshes)if(sm.name==dm.name&&sm.vertices.size()==dm.vertices.size())for(size_t i=0;i<sm.vertices.size();++i)for(int k=0;k<4;++k)if(sm.vertices[i].bones[k]==a.shoulder&&sm.vertices[i].weights[k]>.9f){center+=dm.vertices[i].position;++n;}if(n)center=center/n;std::cout<<"FITTED UPPER "<<side<<' '<<center.x<<','<<center.y<<','<<center.z<<' '<<n<<'\n';}}
    {const auto pose=native.samplePose(0,0);int shown=0;for(const auto& m:native.meshes)if(!m.viewmodelWeapon)for(size_t i=0;i<m.vertices.size();++i){const auto& v=m.vertices[i];scene::Vec3 p{};for(int k=0;k<4;++k)if(v.weights[k]>0)p+=scene::transformPoint(pose[v.bones[k]]*native.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];if(p.x<=0)continue;const float x=480-p.y/p.x*352,y=270-p.z/p.x*352;if(x>510&&x<575&&y>425&&y<460&&shown++<12){std::cout<<"CAP "<<m.name<<' '<<i<<" fitted "<<v.position.x<<','<<v.position.y<<','<<v.position.z;for(const auto& sm:skin.meshes)if(sm.name==m.name&&i<sm.vertices.size()){const auto& sv=sm.vertices[i];std::cout<<" source "<<sv.position.x<<','<<sv.position.y<<','<<sv.position.z;for(int k=0;k<4;++k)if(sv.weights[k]>0)std::cout<<' '<<skin.skeleton.bones[sv.bones[k]].name<<':'<<sv.weights[k];}std::cout<<'\n';}}}
    {std::map<std::array<long long,3>,std::vector<scene::Vec3>> seams;
     for(const auto& sm:skin.meshes)for(const auto& dm:native.meshes)if(sm.name==dm.name&&sm.vertices.size()==dm.vertices.size())for(std::size_t i=0;i<sm.vertices.size();++i){
        const auto p=scene::transformPoint(sm.modelTransform,sm.vertices[i].position);
        seams[{std::llround(p.x*100000),std::llround(p.y*100000),std::llround(p.z*100000)}].push_back(dm.vertices[i].position);
     }
     float gap{};std::size_t split{},duplicates{};
     for(const auto& [key,points]:seams)if(points.size()>1){++duplicates;float local{};for(const auto& a:points)for(const auto& b:points)local=std::max(local,scene::length(a-b));gap=std::max(gap,local);split+=local>.01f;}
     std::cout<<"SEAMS duplicate_positions="<<duplicates<<" split_over_0.01="<<split<<" max_gap="<<gap<<'\n';}
    std::size_t changed{},nonfinite{},invalid{};
    for(int i=0;i<21;++i){const auto pose=native.samplePose(0,native.animations[0].durationFrames*i/20.f);for(std::size_t b=0;b<pose.size();++b){if(b<reference[i].size())changed+=pose[b].v!=reference[i][b].v;for(float x:pose[b].v)nonfinite+=!std::isfinite(x);}}
    for(const auto& m:native.meshes)for(const auto& v:m.vertices)for(int k=0;k<4;++k)if(v.weights[k]>0)invalid+=v.bones[k]>=native.skeleton.bones.size();
    std::cout<<"bones="<<bones<<" changed_driver_matrices="<<changed<<" nonfinite="<<nonfinite<<" invalid_skin_indices="<<invalid<<" meshes="<<native.meshes.size()<<'\n';
    if(changed||nonfinite||invalid)return 1;
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* w=glfwCreateWindow(960,540,"Imported viewhands fit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
    render::StageRenderer renderer;if(!renderer.initialize(error)||!renderer.loadScene(native,error)){std::cerr<<error;return 2;}renderer.setDebugView(1);renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});std::filesystem::create_directories(argv[5]);
    for(const auto frame:{0.f,.5f,1.f}){const auto pose=native.samplePose(0,native.animations[0].durationFrames*frame);scene::Vec3 origin{},forward{1,0,0},up{0,0,1};
        if(const auto camera=native.skeleton.boneByCanonicalName.find("tag_camera");camera!=native.skeleton.boneByCanonicalName.end()&&camera->second<pose.size()){const auto& m=pose[camera->second];origin={m.v[12],m.v[13],m.v[14]};forward=scene::normalize(scene::Vec3{m.v[0],m.v[1],m.v[2]});up=scene::normalize(scene::Vec3{m.v[8],m.v[9],m.v[10]});}
        const auto vp=scene::perspective(75*scene::kPi/180,960.f/540.f,.02f,5000)*scene::lookAtDirection(origin,forward,up);renderer.loadScene(native,error);renderer.render(native,pose,vp,960,540,false,false,false);if(!renderer.saveColorPng(std::filesystem::path(argv[5])/("frame"+std::to_string(frame)+".png"),error)){std::cerr<<error;return 2;}renderer.loadScene(baseline,error);renderer.render(baseline,pose,vp,960,540,false,false,false);if(!renderer.saveColorPng(std::filesystem::path(argv[5])/("baseline"+std::to_string(frame)+".png"),error))return 2;}
    {std::vector<scene::Mat4> rest;for(const auto& b:skin.skeleton.bones)rest.push_back(b.restGlobal);
     const auto layout=scene::imported::viewLayout(skin);
     for(int side=0;side<2;++side){const auto& arm=layout.arms[side];if(!arm.valid())continue;
        const auto p=[&](int b){return scene::transformPoint(rest[b],{});};
        const auto center=(p(arm.wrist)+p(arm.fingers[2][1]))*.5f;
        const auto up=scene::normalize(p(arm.fingers[2][0])-p(arm.wrist));
        const auto normal=scene::normalize(scene::cross(p(arm.fingers[1][0])-p(arm.wrist),p(arm.fingers[4][0])-p(arm.wrist)));
        const float distance=scene::length(p(arm.fingers[2][2])-p(arm.wrist))*2.2f;
        if(!renderer.loadScene(skin,error))return 2;
        const auto vp=scene::perspective(55*scene::kPi/180,960.f/540.f,.02f,5000)*scene::lookAtDirection(center+normal*distance,normal*-1.f,up);
        renderer.render(skin,rest,vp,960,540,false,false,false);
        if(!renderer.saveColorPng(std::filesystem::path(argv[5])/("original_hand"+std::to_string(side)+".png"),error))return 2;
     }}
    {auto handsOnly=native;std::erase_if(handsOnly.meshes,[](const auto& m){return m.viewmodelWeapon;});
     auto referenceHands=baseline;std::erase_if(referenceHands.meshes,[](const auto& m){return m.viewmodelWeapon;});
     const auto layout=scene::imported::viewLayout(native);const auto pose=native.samplePose(0,0);
     for(int side=0;side<2;++side){const auto& arm=layout.arms[side];if(!arm.valid())continue;
        const auto p=[&](int b){return scene::transformPoint(pose[b],{});};
        const auto center=(p(arm.wrist)+p(arm.fingers[2][0]))*.5f;
        const auto up=scene::normalize(p(arm.fingers[2][0])-p(arm.wrist));
        const auto normal=scene::normalize(scene::cross(p(arm.fingers[1][0])-p(arm.wrist),p(arm.fingers[4][0])-p(arm.wrist)));
        const float distance=scene::length(p(arm.fingers[2][0])-p(arm.wrist))*3.8f;
        if(!renderer.loadScene(handsOnly,error))return 2;
        for(int facing:{-1,1}){const auto vp=scene::perspective(55*scene::kPi/180,960.f/540.f,.02f,5000)*scene::lookAtDirection(center+normal*(distance*facing),normal*float(-facing),up);
            if(!renderer.loadScene(handsOnly,error))return 2;
            renderer.render(handsOnly,pose,vp,960,540,false,false,false);
            if(!renderer.saveColorPng(std::filesystem::path(argv[5])/("fitted_hand"+std::to_string(side)+"_side"+std::to_string(facing)+".png"),error))return 2;
            if(!renderer.loadScene(referenceHands,error))return 2;
            renderer.render(referenceHands,pose,vp,960,540,false,false,false);
            if(!renderer.saveColorPng(std::filesystem::path(argv[5])/("driver_hand"+std::to_string(side)+"_side"+std::to_string(facing)+".png"),error))return 2;
        }
     }}
    renderer.shutdown();glfwDestroyWindow(w);glfwTerminate();return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
