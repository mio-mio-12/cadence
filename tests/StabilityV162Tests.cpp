#include "take/Take.h"
#include "take/AtomicFile.h"
#include "take/MemoryUsage.h"
#include "app/BackgroundLoad.h"
#include "app/WorldActionBlend.h"
#include "render/ActorBounds.h"
#include <iostream>
#include <fstream>
#include <limits>
#include <cstdlib>
#include <new>

static bool guardLargeAllocations=false;
void* operator new(std::size_t n){if(guardLargeAllocations&&n>32u*1024u*1024u)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
template<class T> void put(std::ofstream& f,const T& v){f.write(reinterpret_cast<const char*>(&v),sizeof(v));}
static std::string bytes(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
    int failures=0;const auto check=[&](bool ok,const char* why){if(!ok){std::cerr<<"FAIL "<<why<<"\n";++failures;}};
    const auto directory=std::filesystem::temp_directory_path()/("cadence-v162-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);const auto file=directory/"take.c_dm";
    take::Take source;source.boneCount=2;source.actor.baseModel="hands";source.worldActor.baseModel="player";source.worldBoneCount=2;source.botActor.baseModel="bot";source.botBoneCount=2;source.botCount=1;
    for(int i=0;i<4;++i){take::Sample s;s.time=float(i);s.pose={scene::Mat4::identity(),scene::translation({float(i),1,0})};s.worldActor.pose=s.pose;take::RecordedActorState b;b.pose=s.pose;b.primaryClip="walk";s.bots={b};source.samples.push_back(std::move(s));}
    std::string error;check(take::save(source,file,error),"save valid take");const auto originalBytes=bytes(file);
    auto broken=source;broken.samples[2].pose.clear();check(!take::save(broken,file,error),"reject wrong pose count");check(bytes(file)==originalBytes,"failed save preserves every original byte");
    check(!take::detail::atomicFile(file,error,[&](const auto& p){std::ofstream f(p);f<<"partial";return false;}),"injected write failure");
    check(bytes(file)==originalBytes,"partial writer preserves original");
    broken=source;broken.samples[1].bots[0].pose[1].v[2]=std::numeric_limits<float>::quiet_NaN();check(!take::save(broken,file,error)&&bytes(file)==originalBytes,"invalid matrices never publish");
#ifdef _WIN32
    auto locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(locked!=INVALID_HANDLE_VALUE,"lock fixture");
    check(!take::save(source,file,error),"replacement failure is reported");if(locked!=INVALID_HANDLE_VALUE)CloseHandle(locked);
    check(bytes(file)==originalBytes,"failed replacement preserves original");
#endif
    take::Take loaded;check(take::load(file,loaded,error),"current-format roundtrip");
    const auto malformed=directory/"malformed.c_dm";
    {std::ofstream f(malformed,std::ios::binary);f.write("IWTAKE01",8);put(f,1u);put(f,30.f);put(f,1u);put(f,864000u);}
    guardLargeAllocations=true;check(!take::load(malformed,loaded,error),"tiny truncated file rejected");guardLargeAllocations=false;
    check(loaded.samples.size()==4,"failed load preserves current take");
    const auto legacy=directory/"legacy.c_dm";
    const auto writeLegacy=[&](bool nanRate,bool nanPose){std::ofstream f(legacy,std::ios::binary);f.write("IWTAKE01",8);put(f,1u);put(f,nanRate?std::numeric_limits<float>::quiet_NaN():30.f);put(f,1u);put(f,1u);put(f,0.f);put(f,scene::Vec3{});for(int i=0;i<5;++i)put(f,0.f);put(f,0u);put(f,0u);auto m=scene::Mat4::identity();if(nanPose)m.v[0]=std::numeric_limits<float>::quiet_NaN();put(f,m);};
    writeLegacy(false,false);check(take::load(legacy,loaded,error),"valid v1 remains supported");
    writeLegacy(true,false);check(!take::load(legacy,loaded,error),"NaN rate rejected");
    writeLegacy(false,true);check(!take::load(legacy,loaded,error),"NaN pose rejected");
    const auto* viewData=source.samples[1].pose.data();const auto* botData=source.samples[1].bots[0].pose.data();
    check(source.trim(1,2),"trim valid range");check(source.samples.size()==2&&source.samples[0].time==0,"trim time unchanged");
    check(source.samples[0].pose.data()==viewData&&source.samples[0].bots[0].pose.data()==botData,"trim moves allocations instead of copying");
    take::MemoryUsage memory;memory.update(source);check(memory.poseBytes==2*3*2*sizeof(scene::Mat4),"memory includes all actor poses");const auto counted=memory.poseBytes;
    memory.update(source);check(memory.poseBytes==counted,"memory accounting does not recount old samples");
    take::Sample extra=source.samples.back();extra.pose.resize(5);source.samples.push_back(std::move(extra));memory.update(source);
    check(memory.poseBytes==counted+9*sizeof(scene::Mat4),"variable slot skeleton size accounted exactly");
    source.clear();memory.update(source);check(memory.poseBytes==0,"memory clear invalidation");
    scene::CastScene rig;rig.skeleton.bones.resize(2);rig.skeleton.bones[1].parent=0;rig.animations.resize(1);
    for(float duration:{0.f,.05f,2.f,20.f,std::numeric_limits<float>::quiet_NaN()}){
        cadence::WorldActionBlend blend;std::vector<scene::Transform> local(2);blend.apply(rig,{},0,.016f,duration,local);
        check(blend.previousMask.size()==2,"initial mask invariant");blend.apply(rig,0,0,.016f,duration,local);blend.apply(rig,{},0,.016f,duration,local);
        check(blend.previousMask.size()==2,"interrupt mask invariant");
    }
    struct Result{std::string error;};
    std::optional<std::future<Result>> pending=std::async(std::launch::async,[]()->Result{throw std::runtime_error("fixture failure");});
    check(!cadence::consumeBackgroundLoad(pending).error.empty()&&!pending,"background exception becomes status");
    std::atomic<bool> completed{};pending=std::async(std::launch::async,[&]{completed=true;return Result{};});cadence::joinBackgroundLoad(pending);
    check(completed&&!pending,"join precedes dependency teardown");
    scene::Mesh mesh;mesh.skinned=true;
    for(int i=0;i<8;++i){scene::Vertex v;v.position={float(i&1)*2-1,float((i>>1)&1)*2-1,float((i>>2)&1)*2-1};v.bones={0,1,0,0};v.weights={.25f,.75f,0,0};mesh.vertices.push_back(v);}
    auto bounds=render::visibility::MeshBounds::build(mesh,2);std::vector<scene::Mat4> pose{scene::translation({20,0,0}),scene::trs({30,5,0},scene::fromAxisAngle({0,0,1},.5f),{2,.5f,-1})};
    const auto model=scene::translation({10,3,0});scene::Bounds transformed;
    check(bounds.append(transformed,rig.skeleton,pose,model,true),"skinned bounds valid");
    for(const auto& v:mesh.vertices){const auto p=scene::transformPoint(model,scene::transformPoint(pose[0],v.position)*.25f+scene::transformPoint(pose[1],v.position)*.75f);
        check(p.x>=transformed.minimum.x&&p.x<=transformed.maximum.x&&p.y>=transformed.minimum.y&&p.y<=transformed.maximum.y&&p.z>=transformed.minimum.z&&p.z<=transformed.maximum.z,"bounds contain every weighted vertex");}
    const auto frustum=scene::extractFrustum(scene::Mat4::identity());check(!render::visibility::visible(transformed,frustum),"offscreen bounds culled");
    check(render::visibility::visible({},frustum),"uncertain bounds fail open");
    mesh.vertices[0].weights[0]=-1;check(!render::visibility::MeshBounds::build(mesh,2).trustworthy,"unsupported weights fail open");
    for(const auto& entry:std::filesystem::directory_iterator(directory)){check(entry.is_regular_file(),"no staging directories leaked");if(entry.is_regular_file())std::filesystem::remove(entry.path());}std::filesystem::remove(directory);
    std::cout<<(failures?"FAILED":"PASS")<<" v162 stability, replay storage, bounds and lifecycle regressions\n";return failures?1:0;
}
