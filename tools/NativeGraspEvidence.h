#pragma once
// Diagnostic only. No importer or runtime caller includes this file.
inline void inspectNativeGraspGeometry(const scene::CastScene& rig,const std::vector<scene::Mat4>& pose,const std::array<scene::Vec3,2>& contacts){
    if(rig.muzzleAnchors.size()!=1)return;
    std::vector<scene::Vec3> points;
    for(const auto& mesh:rig.meshes)if(mesh.viewmodelWeapon)for(const auto& v:mesh.vertices){auto p=v.position;
        if(mesh.skinned){scene::Vec3 total{};float w{};for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<pose.size()){total=total+scene::transformPoint(pose[v.bones[k]]*rig.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];w+=v.weights[k];}if(w>0)p=total/w;}
        points.push_back(scene::transformPoint(mesh.modelTransform,p));}
    if(points.size()<3)return;
    scene::Vec3 center{};for(auto p:points)center=center+p;center=center/float(points.size());
    float c[3][3]{};for(auto p:points){p=p-center;const float x[]{p.x,p.y,p.z};for(int i=0;i<3;++i)for(int j=0;j<3;++j)c[i][j]+=x[i]*x[j]/float(points.size());}
    const auto mul=[&](scene::Vec3 x){return scene::Vec3{c[0][0]*x.x+c[0][1]*x.y+c[0][2]*x.z,c[1][0]*x.x+c[1][1]*x.y+c[1][2]*x.z,c[2][0]*x.x+c[2][1]*x.y+c[2][2]*x.z};};
    scene::Vec3 axis{};float variance{},secondary{};
    for(auto s:{scene::Vec3{1,0,0},scene::Vec3{0,1,0},scene::Vec3{0,0,1}}){for(int i=0;i<32;++i)s=scene::normalize(mul(s));const float v=scene::dot(s,mul(s));if(v>variance){axis=s;variance=v;}}
    for(auto s:{scene::Vec3{1,0,0},scene::Vec3{0,1,0},scene::Vec3{0,0,1}}){s=scene::normalize(s-axis*scene::dot(s,axis));for(int i=0;i<32;++i){auto v=mul(s);s=scene::normalize(v-axis*scene::dot(v,axis));}secondary=std::max(secondary,scene::dot(s,mul(s)));}
    const auto& anchor=rig.muzzleAnchors.front();if(anchor.bone>=pose.size())return;const auto tip=scene::transformPoint(pose[anchor.bone],anchor.local);if(scene::dot(tip-center,axis)<0)axis=axis*-1.f;
    float front=-INFINITY,rear=INFINITY;for(auto p:points){const float x=scene::dot(p-center,axis);front=std::max(front,x);rear=std::min(rear,x);}
    const float muzzle=scene::dot(tip-center,axis),l=scene::dot(contacts[0]-tip,axis),r=scene::dot(contacts[1]-tip,axis);
    std::cout<<"gun_vertices="<<points.size()<<" principal_axis="<<axis.x<<','<<axis.y<<','<<axis.z<<" variance_ratio="<<variance/std::max(secondary,.000001f)<<" muzzle_to_front="<<front-muzzle<<" gun_length="<<front-rear<<" left_behind_tip="<<l<<" right_behind_tip="<<r<<'\n';
    for(std::size_t b=0;b<rig.skeleton.bones.size();++b){const auto& n=rig.skeleton.bones[b].name;if(n.find("muzzle_flash")==std::string::npos&&n!="dew2cast_weapon__gun")continue;const auto p=scene::transformPoint(pose[b],{});std::cout<<"socket="<<n<<" position="<<p.x<<','<<p.y<<','<<p.z<<" axis_dots=";for(int k=0;k<3;++k){const auto& m=pose[b];std::cout<<scene::dot(scene::normalize(scene::Vec3{m.v[k*4],m.v[k*4+1],m.v[k*4+2]}),axis)<<',';}std::cout<<'\n';}
    std::cout<<"geometry_right_rear_candidate="<<(variance>3.f*secondary&&std::abs(front-muzzle)<(front-rear)*.1f&&r+1.f<l&&r<0&&l<0)<<" visual_confirmation_required=1\n";
    if(const auto* out=std::getenv("CADENCE_AUDIT_ELIGIBILITY_IMAGES")){
        std::filesystem::create_directories(out);if(!glfwInit())throw std::runtime_error("GLFW init");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(1000,750,"Native grasp evidence",nullptr,nullptr);if(!window)throw std::runtime_error("GLFW window");glfwMakeContextCurrent(window);
        render::StageRenderer renderer;std::string error;if(!renderer.initialize(error)||!renderer.loadScene(rig,error))throw std::runtime_error(error);renderer.setDebugView(1);renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
        const auto focus=(contacts[0]+contacts[1])*.5f;const float distance=std::max(20.f,(front-rear)*1.3f);auto lateral=scene::normalize(scene::cross(axis,{0,0,1}));if(scene::length(lateral)<.5f)lateral={0,1,0};
        for(int side=0;side<3;++side){const auto eye=focus+(side==2?axis*-distance:lateral*(side?distance:-distance))+scene::Vec3{0,0,distance*.25f};const auto vp=scene::perspective(48*scene::kPi/180,1000.f/750.f,.05f,2000)*scene::lookAt(eye,focus,{0,0,1});renderer.render(rig,pose,vp,1000,750,false,false,false);
            renderer.renderDebugLine3D(tip-axis*5.f,tip,{0,1,0,1},vp);for(int hand=0;hand<2;++hand)renderer.renderDebugLine3D(contacts[hand]-scene::Vec3{0,0,.5f},contacts[hand]+scene::Vec3{0,0,.5f},hand?scene::Vec4{0,1,1,1}:scene::Vec4{1,.3f,0,1},vp);
            if(!renderer.saveColorPng(std::filesystem::path(out)/("native_"+std::to_string(side)+".png"),error))throw std::runtime_error(error);if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Native evidence GL error");}
        renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();}
}
