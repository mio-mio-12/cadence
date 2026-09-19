#include "ReferencePoseV131.h"
#include "scene/PoseEvaluationScratch.h"
#include "render/FramePreparation.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <future>
#include <numeric>
#include <random>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";std::exit(1);}}while(false)
using namespace scene;
static bool equal(const std::vector<Transform>& a,const std::vector<Transform>& b){
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i){
        if(std::memcmp(&a[i].position,&b[i].position,sizeof(Vec3))||std::memcmp(&a[i].rotation,&b[i].rotation,sizeof(Quat))||std::memcmp(&a[i].scale,&b[i].scale,sizeof(Vec3)))return false;
        const auto am=trs(a[i].position,a[i].rotation,a[i].scale),bm=trs(b[i].position,b[i].rotation,b[i].scale);
        if(std::memcmp(am.data(),bm.data(),sizeof(Mat4)))return false;
    }return true;
}
static bool equal(const std::vector<Mat4>& a,const std::vector<Mat4>& b){return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(Mat4))==0);}
static CastScene fixture(){
    CastScene s;
    for(int i=0;i<102;++i){Bone b;b.parent=i?((i-1)/3):-1;b.name=i%11==0?"tag_magazine":i%13==0?"tag_ads":"joint_"+std::to_string(i);b.restLocal.position={.1f*i,.2f,.3f};s.skeleton.bones.push_back(b);}
    for(int a=0;a<7;++a){Animation clip;clip.looping=a%2;clip.durationFrames=60;
        for(int b=0;b<102;++b)for(int c=0;c<7;++c){
            Track t;t.boneIndex=b;t.property=static_cast<TrackProperty>(c);t.mode=static_cast<TrackMode>(a%3);t.ownsLayer=(b+a)%9!=0;t.frames={0,17,60};t.additiveWeight=.3f;
            if(c==3)t.rotationValues={Quat{},fromEulerRadians({.03f*a,.02f*b,.1f}),fromEulerRadians({.1f,.01f*b,.2f})};
            else t.scalarValues={c<3?.1f*b:1.f,c<3?.3f*a:1.2f,c<3?.2f*b:.9f};
            clip.tracks.push_back(t);
        }s.animations.push_back(std::move(clip));
    }return s;
}
static void poses(){
    auto s=fixture();ReferenceSceneV131 old;static_cast<CastScene&>(old)=s;
    std::vector<PoseSlot> slots(2);
    for(int i=0;i<6;++i){PoseLayer n;n.animation=i+1;n.weight=.17f*(i+1);n.mode=i%2?LayerMode::Additive:LayerMode::Override;n.preserveWeaponMechanisms=true;n.preserveViewmodelAimRoot=true;n.suppressRootMotion=i%2;n.referenceAnimation=2;n.referenceFrame=8;slots[i%2].nodes.push_back(n);}
    for(int step=0;step<170;++step){
        const float frame=float((step*37)%91)-10.25f;
        for(auto& slot:slots)for(auto& n:slot.nodes)n.frame=frame+n.animation;
        if(step==30){s.animations[2].tracks[20].boneIndex=6;s.animations[2].tracks[21].property=TrackProperty::ScaleZ;s.animations[2].tracks[22].ownsLayer=false;}
        if(step==40)s.skeleton.bones[11].name="ordinary_11";
        if(step==50)s.skeleton.bones[9].name="tag_torso";
        if(step==60)s.animations[2].tracks.pop_back();
        if(step==70)s.animations[2]=s.animations[3];
        if(step==80){s.skeleton.bones[9].restLocal.position.x=8;s.skeleton.bones[10].parent=2;}
        if(step==90)s.animations.push_back(s.animations[1]);
        if(step==100)slots[0].nodes[1].weight=0;
        if(step==110)slots[0].nodes[1].weight=.7f;
        if(step==120)slots[1].nodes[0].referenceAnimation=999;
        static_cast<CastScene&>(old)=s;
        CHECK(equal(s.sampleLocalPoseSlots(0,frame,slots),old.sampleLocalPoseSlots(0,frame,slots)));
        CHECK(equal(s.samplePoseSlots(0,frame,slots),old.samplePoseSlots(0,frame,slots)));
        CHECK(equal(s.sampleLayerStack(0,frame,slots[0].nodes),old.sampleLayerStack(0,frame,slots[0].nodes)));
        for(auto mode:{LayerMode::Additive,LayerMode::Override})CHECK(equal(s.sampleLayeredPose(0,frame,1,frame+.3f,.6f,mode,true),old.sampleLayeredPose(0,frame,1,frame+.3f,.6f,mode,true)));
        std::vector<Transform> output(300);s.sampleLocalPoseInto(0,frame,output);CHECK(equal(output,s.sampleLocalPose(0,frame)));
    }
    // Recursion leases never overwrite storage still in use by an outer evaluator.
    pose_detail::Lease outer;outer.workspace.samples.resize(1);outer.workspace.samples[0].weight=123;
    {pose_detail::Lease inner;CHECK(&inner.workspace!=&outer.workspace);inner.workspace.samples.resize(1);inner.workspace.samples[0].weight=456;}
    CHECK(outer.workspace.samples[0].weight==123);
    volatile float sink=0;const int loops=1200;
    auto bench=[&](const auto& evaluator){const auto start=std::chrono::steady_clock::now();for(int n=0;n<loops;++n){auto p=evaluator.sampleLocalPoseSlots(0,float(n%60),slots);sink=p[1].position.x;}return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
    const auto oldMs=bench(old),newMs=bench(s);
    std::cout<<"Pose preparation microbenchmark ("<<loops<<" evaluations): v131="<<oldMs<<"ms v132="<<newMs<<"ms\n";
    for(auto& slot:slots)for(auto& n:slot.nodes)n.preserveWeaponMechanisms=n.preserveViewmodelAimRoot=false;
    const auto plainOld=bench(old),plainNew=bench(s);
    std::cout<<"Without mechanism/aim filtering: v131="<<plainOld<<"ms v132="<<plainNew<<"ms\n";
}
static std::vector<float> referenceVisibility(const Skeleton& s,const std::unordered_set<std::size_t>& hidden,std::size_t count){
    std::vector<float> out(count,1);for(std::size_t i=0;i<std::min(count,s.bones.size());++i){auto bone=static_cast<int>(i);while(bone>=0){if(hidden.contains(bone)){out[i]=0;break;}bone=s.bones[bone].parent;}}return out;
}
static void visibility(){
    auto s=fixture();render::VisibilityPreparation cache;std::vector<float> shown;
    for(int i=0;i<150;++i){
        if(i%3==0)s.hiddenBones={std::size_t(i%102)};if(i%7==0)s.hiddenBones.clear();if(i==30)s.skeleton.bones[6].parent=1;
        const auto count=i%8==0?1:s.skeleton.bones.size();cache.update(s.skeleton,s.hiddenBones,count,shown);
        CHECK(shown==referenceVisibility(s.skeleton,s.hiddenBones,count));CHECK(!cache.update(s.skeleton,s.hiddenBones,count,shown));
    }
    s.skeleton.bones.clear();cache.update(s.skeleton,{},1,shown);CHECK(shown==std::vector<float>{1});
}
static void sorting(){
    struct Mesh {Vec3 min,max;int queue;bool explicitPolicy;render::SurfaceSortKey sortKey;};
    std::mt19937 random(42);std::vector<Mesh> meshes;
    for(int i=0;i<700;++i){Vec3 p{float(random()%60),float(random()%60),float(random()%60)};Mesh m{p,p+Vec3{4,5,6},i%3?3000:2000,i%7==0};m.sortKey={(m.min+m.max)*.5f,m.queue,m.explicitPolicy};meshes.push_back(m);}
    std::vector<std::size_t> old(meshes.size());std::iota(old.begin(),old.end(),0);auto optimized=old;std::vector<float> distance;
    for(int n=0;n<100;++n){
        Vec3 camera{float(n%13),float(n%23),float(n%9)};
        std::stable_sort(old.begin(),old.end(),[&](auto a,auto b){const auto& x=meshes[a];const auto& y=meshes[b];if(x.queue!=y.queue)return x.queue<y.queue;auto xc=(x.min+x.max)*.5f-camera,yc=(y.min+y.max)*.5f-camera;return dot(xc,xc)>dot(yc,yc);});
        render::sortTransparentPass(optimized,meshes,camera,distance);CHECK(old==optimized);
    }
    for(auto& m:meshes)m.sortKey.explicitPolicy=false;const auto before=optimized;render::sortTransparentPass(optimized,meshes,{},distance);CHECK(optimized==before);
}
int main(){
    poses();auto other=std::async(std::launch::async,poses);other.get();visibility();sorting();
    render::DriverPollInterval poll;auto now=render::DriverPollInterval::Clock::now();CHECK(poll.due(now));
    for(int n=1;n<1000;++n)CHECK(!poll.due(now+std::chrono::milliseconds(n)));CHECK(poll.due(now+std::chrono::seconds(1)));
    std::cout<<"PASS bit-identical v131 layered/slot poses, mutations/seeks, thread isolation, visibility, sorting and polling\n";
}
