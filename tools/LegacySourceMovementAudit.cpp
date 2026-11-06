// Exercise the actual application controller with deterministic input, without
// creating a graphics context or injecting input into the user's desktop.
#define NOMINMAX
#include <Windows.h>
#include <GLFW/glfw3.h>
#include <set>
static std::set<int> auditKeys;
static int auditKey(GLFWwindow*,int key){return auditKeys.contains(key)?GLFW_PRESS:GLFW_RELEASE;}
static int auditMouse(GLFWwindow*,int){return GLFW_RELEASE;}
static int auditWindow(GLFWwindow*,int){return GLFW_TRUE;}
static int auditInput(GLFWwindow*,int){return GLFW_CURSOR_DISABLED;}
static void auditCursor(GLFWwindow*,double* x,double* y){*x=*y=0;}
static SHORT auditAsync(int){return 0;}
#define glfwGetKey auditKey
#define glfwGetMouseButton auditMouse
#define glfwGetWindowAttrib auditWindow
#define glfwGetInputMode auditInput
#define glfwGetCursorPos auditCursor
#define GetAsyncKeyState auditAsync
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#undef GetAsyncKeyState
#undef glfwGetCursorPos
#undef glfwGetInputMode
#undef glfwGetWindowAttrib
#undef glfwGetMouseButton
#undef glfwGetKey
#include "gameplay/PlayerMapMovement.h"

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void auditNear(scene::Vec3 a,scene::Vec3 b,const char* message){if(scene::length(a-b)>.002f){std::cerr<<message<<" got "<<a.x<<','<<a.y<<','<<a.z<<" expected "<<b.x<<','<<b.y<<','<<b.z<<'\n';throw std::runtime_error(message);}}
static void quad(scene::glb::Map& map,scene::Vec3 a,scene::Vec3 b,scene::Vec3 c,scene::Vec3 d){
    for(auto p:{std::array{a,b,c},std::array{a,c,d}}){scene::glb::CollisionTriangle t;t.a=p[0];t.b=p[1];t.c=p[2];t.normal=scene::normalize(scene::cross(t.b-t.a,t.c-t.a));t.minimum=t.maximum=t.a;
        for(auto v:p){t.minimum={std::min(t.minimum.x,v.x),std::min(t.minimum.y,v.y),std::min(t.minimum.z,v.z)};t.maximum={std::max(t.maximum.x,v.x),std::max(t.maximum.y,v.y),std::max(t.maximum.z,v.z)};}
        t.walkable=t.normal.z>=.7f;t.blocking=!t.walkable;map.collision.push_back(t);
    }
}
static scene::glb::Map floorMap(bool step=false){scene::glb::Map m;quad(m,{-10000,-10000,0},{10000,-10000,0},{10000,10000,0},{-10000,10000,0});
    if(step){quad(m,{200,-1000,0},{200,-1000,20},{200,1000,20},{200,1000,0});quad(m,{200,-1000,20},{2000,-1000,20},{2000,1000,20},{200,1000,20});}m.buildCollisionIndex();return m;}
static std::unique_ptr<AppState> actor(const scene::glb::Map& map,scene::Vec3 p,bool grounded){auto a=std::make_unique<AppState>();a->window=reinterpret_cast<GLFWwindow*>(1);a->actorInputCaptured=true;a->actorCursorInitialized=true;a->actorFollowCamera=false;a->movementAlgorithm=a->previousMovementAlgorithm=1;a->actorPosition=a->actorPreviousPosition=a->actorRenderPosition=p;a->actorGrounded=grounded;a->loadedMap=map;a->exoDoubleJumpEnabled=false;a->traversalSettings.airEdgeEnabled=false;return a;}
static void tick(AppState& a){a.gameplayClock+=.008f;updateActorController(a,.008f);require(!a.actorMantling,"unexpected mantle in controller fixture");}

// Frozen pre-change GROUNDED Source-v1 integration. The change under review
// must not replace its terrain support/step path with the swept controllers.
static void baselineGround(const AppState& app,scene::Vec3& p,scene::Vec3& v){
    constexpr float dt=.008f;const auto old=p;const float speed=std::hypot(v.x,v.y);
    if(speed>0){const float next=std::max(0.f,speed-std::max(speed,gameplay::iw::kStopSpeed)*4.f*dt);v.x*=next/speed;v.y*=next/speed;}
    const auto wish=app.actorWishVelocity;const float wishSpeed=scene::length(wish);
    if(wishSpeed>0){const auto direction=wish/wishSpeed;const float add=wishSpeed-scene::dot(scene::Vec3{v.x,v.y,0},direction);if(add>0){const float amount=std::min(add,10.f*dt*wishSpeed);v.x+=amount*direction.x;v.y+=amount*direction.y;}}
    v.z-=gameplay::iw::kGravity*dt;const auto attempted=p+v*dt;scene::Vec3 normal{};p=constrainActorMove(app,old,attempted,&normal);p.z=std::max(p.z,attempted.z);
    if(scene::length(normal)>.5f){normal=scene::normalize(normal);const float inward=scene::dot(v,normal);if(inward<0)v-=normal*inward;}
    const float ground=actorGroundHeight(app,p.x,p.y,old.z+scene::course::kStepHeight+4.f);
    require(ground>-1e8f&&p.z-ground<=scene::course::kStepHeight,"baseline fixture unexpectedly airborne");p.z=ground;if(v.z<0)v.z=0;
}
int main(int argc,char** argv)try{
    ImGui::CreateContext();
    if(argc>3&&std::string(argv[1])=="--rectangular-map"){
        const auto output=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(output);std::ofstream report(output/"map-routes.csv"),frames(output/"map-frames.csv");report<<"geometry,route,mode,x,y,z,heading,expected_step,progress,embedded_ticks,result\n";frames<<"geometry,route,mode,tick,x,y,z,grounded,startsolid\n";
        for(bool visible:{false,true}){scene::glb::Map map;std::string error;scene::c2m::LoadOptions options;options.renderGeometryCollision=visible;require(scene::c2m::load(argv[2],map,error,1.f,nullptr,options),error.c_str());const auto label=visible?"visible":"authored_requested";std::cout<<"MAP "<<label<<" triangles="<<map.collision.size()<<" authored="<<map.authoredCollision.present<<'\n';
            struct Route{scene::Vec3 start,direction;float step;};std::vector<Route> routes;const auto stride=std::max(std::size_t(1),map.collision.size()/24000);
            for(std::size_t i=0;i<map.collision.size()&&routes.size()<12;i+=stride){const auto& triangle=map.collision[i];if(!triangle.walkable||triangle.normal.z<.98f)continue;const std::array<scene::Vec3,3> vertices{triangle.a,triangle.b,triangle.c};
                for(int edge=0;edge<3&&routes.size()<12;++edge){const auto middle=(vertices[edge]+vertices[(edge+1)%3])*.5f,delta=vertices[(edge+1)%3]-vertices[edge];const auto length=std::hypot(delta.x,delta.y);if(length<8)continue;
                    for(float sign:{-1.f,1.f}){const scene::Vec3 outward{sign*delta.y/length,-sign*delta.x/length,0};const float extent=std::abs(outward.x)*43+std::abs(outward.y)*37;auto start=middle+outward*(extent+2.f);const float floor=map.groundHeight(start.x,start.y,middle.z-45.f-.1f,-1e9f),rise=middle.z-floor;if(floor<-1e8f||rise<1||rise>scene::course::kStepHeight-1)continue;start.z=floor;if(gameplay::movement::sweep(map,start,start,scene::Vec2{43,37},164.9f).startSolid)continue;
                        auto end=start-outward*6.f;end.z-=.1f;const auto stepped=map.constrainMove(start,end,scene::Vec2{43,37},164.9f,scene::course::kStepHeight);if(stepped.z<floor+.8f||scene::length(scene::Vec3{stepped.x-end.x,stepped.y-end.y,0})>.1f)continue;
                        const float center=map.navigationGroundHeight(stepped.x,stepped.y,floor+scene::course::kStepHeight+4,-1e9f,scene::course::kPlayerRadius*.75f);if(center>=stepped.z-.5f)continue;bool duplicate=false;for(const auto& r:routes)duplicate|=scene::length(r.start-start)<60;if(duplicate)continue;routes.push_back({start,-outward,stepped.z});if(routes.size()>=12)break;
                    }
                }
            }
            std::cout<<"MAP "<<label<<" support-disagreement candidates="<<routes.size()<<'\n';
            for(std::size_t route=0;route<routes.size();++route)for(int mode:{0,1}){const auto& r=routes[route];auto a=actor(map,r.start,true);a->movementAlgorithm=a->previousMovementAlgorithm=mode;a->playerClipWidthCm=86;a->playerClipDepthCm=74;a->playerClipHeightCm=164.9f;a->actorYaw=std::atan2(r.direction.y,r.direction.x);a->actorVelocity=r.direction*250;auditKeys={GLFW_KEY_W};int embedded=0;for(int tickIndex=0;tickIndex<24;++tickIndex){tick(*a);const auto p=a->actorPosition;const bool solid=gameplay::movement::sweep(map,p,p,scene::Vec2{43,37},164.9f).startSolid;embedded+=solid;frames<<label<<','<<route<<','<<mode<<','<<tickIndex<<','<<p.x<<','<<p.y<<','<<p.z<<','<<a->actorGrounded<<','<<solid<<'\n';}const float progress=scene::dot(a->actorPosition-r.start,r.direction);report<<label<<','<<route<<','<<mode<<','<<r.start.x<<','<<r.start.y<<','<<r.start.z<<','<<a->actorYaw<<','<<r.step<<','<<progress<<','<<embedded<<','<<(embedded?"EMBEDDED":progress<10?"BLOCKED_REVIEW":"PROGRESSED")<<'\n';std::cout<<"MAP "<<label<<" route="<<route<<" mode="<<mode<<" progress="<<progress<<" embedded="<<embedded<<'\n';}
        }
        std::ofstream(output/"completed.txt")<<"Bounded deterministic actual-map support-disagreement probes complete. Blocked routes may have legitimate obstructions; CSV is not visual certification. Zero candidates is no coverage, not a pass.\n";ImGui::DestroyContext();return 0;
    }
    if(argc>2&&std::string(argv[1])=="--rectangular"){
        const auto output=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(output);std::ofstream csv(output/"rectangular-controller.csv");csv<<"mode,fixture,tick,x,y,z,vx,vy,vz,grounded,startsolid,center_ground,full_support\n";bool passed=true;
        for(int mode:{0,1})for(bool corner:{false,true}){
            auto map=floorMap();const float lowY=corner?30.f:-100.f,highY=corner?50.f:100.f;
            quad(map,{100,lowY,0},{100,lowY,10},{100,highY,10},{100,highY,0});quad(map,{100,lowY,10},{130,lowY,10},{130,highY,10},{100,highY,10});quad(map,{130,lowY,0},{130,highY,0},{130,highY,10},{130,lowY,10});map.buildCollisionIndex();
            auto a=actor(map,{55,0,0},true);a->movementAlgorithm=a->previousMovementAlgorithm=mode;a->playerClipWidthCm=86;a->playerClipDepthCm=74;a->playerClipHeightCm=164.9f;a->actorVelocity={250,0,0};auditKeys={GLFW_KEY_W};bool edgeSupport=false;int embedded=0;
            for(int frame=0;frame<100;++frame){tick(*a);const auto p=a->actorPosition;const auto solid=gameplay::movement::sweep(map,p,p,scene::Vec2{43,37},164.9f).startSolid;const auto support=gameplay::movement::playerSupport(map,p,scene::Vec2{43,37},164.9f,scene::course::kStepHeight);embedded+=solid;edgeSupport|=p.x<71&&p.x>57&&p.z>=9.9f;csv<<mode<<','<<(corner?"corner_prop":"full_curb")<<','<<frame<<','<<p.x<<','<<p.y<<','<<p.z<<','<<a->actorVelocity.x<<','<<a->actorVelocity.y<<','<<a->actorVelocity.z<<','<<a->actorGrounded<<','<<solid<<','<<actorGroundHeight(*a,p.x,p.y,p.z+scene::course::kStepHeight)<<','<<support.value_or(-9999)<<'\n';}
            const bool ok=edgeSupport&&embedded==0&&a->actorPosition.x>180;passed&=ok;std::cout<<"RECT mode="<<mode<<" fixture="<<(corner?"corner_prop":"full_curb")<<" edge_support="<<edgeSupport<<" embedded_ticks="<<embedded<<" end_x="<<a->actorPosition.x<<" result="<<(ok?"PASS":"FAIL")<<'\n';
            // Raising the full body over this step is forbidden by a low ceiling.
            quad(map,{-500,-500,169.9f},{500,-500,169.9f},{500,500,169.9f},{-500,500,169.9f});map.buildCollisionIndex();auto low=actor(map,{55,0,0},true);low->movementAlgorithm=low->previousMovementAlgorithm=mode;low->playerClipWidthCm=86;low->playerClipDepthCm=74;low->playerClipHeightCm=164.9f;low->actorVelocity={250,0,0};for(int frame=0;frame<12;++frame){tick(*low);if(low->actorPosition.z>5.01f)passed=false;}
            auto air=actor(floorMap(),{60,0,30},false);air->movementAlgorithm=air->previousMovementAlgorithm=mode;air->playerClipWidthCm=86;air->playerClipDepthCm=74;air->playerClipHeightCm=164.9f;air->actorVelocity={0,0,100};auditKeys.clear();tick(*air);const bool airborne=air->actorPosition.z>30&&!air->actorGrounded;passed&=airborne;std::cout<<"RECT mode="<<mode<<" ceiling_z="<<low->actorPosition.z<<" airborne_no_snap="<<airborne<<'\n';
        }
        csv.flush();std::ofstream(output/"result.txt")<<(passed?"PASS":"FAIL")<<" actual Default/Source Legacy controller 86x74x164.9 cm, low curb/corner, low ceiling and rising airborne checks\n";ImGui::DestroyContext();return passed?0:1;
    }
    for(bool step:{false,true}){auto a=actor(floorMap(step),{},true);scene::Vec3 p{},v{};auditKeys={GLFW_KEY_W};
        for(int i=0;i<160;++i){tick(*a);baselineGround(*a,p,v);auditNear(a->actorPosition,p,"ground/step position changed");auditNear(a->actorVelocity,v,"ground/step velocity changed");require(a->actorGrounded,"grounded route lost support");}
        if(step)require(a->actorPosition.z>19.9f,"step route did not traverse curb");
    }
    auditKeys.clear();
    scene::glb::Map ramp;quad(ramp,{-1000,-1000,-2000},{1000,-1000,2000},{1000,1000,2000},{-1000,1000,-2000});ramp.buildCollisionIndex();
    const auto n=ramp.collision.front().normal;const float r=scene::course::kPlayerRadius;
    const scene::Vec3 entry{0,0,(r+1.5f)/n.z-r};
    {auto a=actor(ramp,entry,false);a->actorVelocity={0,100,0};const auto expected=entry+scene::Vec3{0,100,-gameplay::iw::kGravity*.008f}*.008f;tick(*a);auditNear(a->actorPosition,expected,"noncontact ramp attracted actor");require(!a->actorSurfing,"nearby uncontacted ramp marked surfing");}
    {auto a=actor(ramp,entry,false);a->actorVelocity=n*200.f;a->actorSurfing=true;const auto expected=entry+(a->actorVelocity+scene::Vec3{0,0,-gameplay::iw::kGravity*.008f})*.008f;tick(*a);auditNear(a->actorPosition,expected,"leaving ramp altered velocity path");require(!a->actorSurfing,"leaving ramp retained sticky surf state");}
    {auto a=actor(ramp,{0,0,r/n.z-r},false);a->actorVelocity=-n*100.f+scene::Vec3{0,200,0};tick(*a);require(a->actorSurfing&&!a->actorGrounded,"actual inward steep contact not identified");require(std::abs(a->actorVelocity.y-200.f)<.002f,"steep collision removed tangent speed");require(scene::dot(a->actorVelocity,n)>-.002f,"steep collision retained inward velocity");}
    {auto a=actor(floorMap(),{0,0,.01f},false);a->actorVelocity={0,0,-10};a->actorSurfing=true;tick(*a);require(a->actorGrounded&&!a->actorSurfing,"stale surf state prevented floor landing");}
    // Switching controllers must not retain another controller's descriptive
    // contact flag, suppress a real landing, or inject/remove airborne speed.
    for(int mode:{0,2}){
        auto a=actor(floorMap(),{0,0,.01f},false);a->movementAlgorithm=mode;
        a->actorVelocity={0,0,-10};a->actorSurfing=true;tick(*a);
        require(a->actorGrounded&&!a->actorSurfing,"switch from Source surf prevented legacy floor landing");
        auto b=actor(floorMap(),{0,0,1000},false);b->movementAlgorithm=mode;
        b->actorVelocity={125,90,0};b->actorSurfing=true;tick(*b);
        require(!b->actorSurfing,"switch retained stale surf state");
        require(std::abs(b->actorVelocity.x-125)<.002f&&std::abs(b->actorVelocity.y-90)<.002f,"switch changed unforced airborne momentum");
    }
    {auto a=actor(floorMap(),{0,0,1000},false),b=actor(floorMap(),{0,0,1000},false);a->actorVelocity={500,90,0};b->actorVelocity={-500,-90,0};for(int i=0;i<30;++i){tick(*a);tick(*b);auditNear(a->actorVelocity,{-b->actorVelocity.x,-b->actorVelocity.y,b->actorVelocity.z},"reverse airborne velocity differs");}require(a->actorVelocity.x==500,"uncapped airborne velocity decayed");}
    {auto a=actor(floorMap(),{},true);auditKeys={GLFW_KEY_SPACE};int hops=0;float previous=0;for(int i=0;i<600;++i){tick(*a);if(previous<=0&&a->actorVelocity.z>0)++hops;previous=a->actorVelocity.z;}require(hops>=3,"held Space did not chain jumps");}
    auditKeys.clear();
    if(argc>1){scene::glb::Map map;std::string error;scene::c2m::LoadOptions options;options.renderGeometryCollision=true;
        require(scene::c2m::load(argv[1],map,error,1.f,nullptr,options),error.c_str());
        require(!map.authoredCollision.present,"fixture must use actual rebuilt visible-mesh collision");
        std::cout<<"Map collision: visible geometry; triangles="<<map.collision.size()<<'\n';
        int totalCompared=0;
        for(auto p:{scene::Vec3{918.1605f,270.4745f,400.2932f},scene::Vec3{-3071.9080f,949.4807f,89.3559f},scene::Vec3{-805.0438f,-1609.4235f,-941.9883f}})for(int direction=0;direction<8;++direction){
            auto a=actor(map,p,true);auto start=p;start.z=actorGroundHeight(*a,p.x,p.y,p.z+scene::course::kStepHeight);require(start.z>-1e8f,"real-map start has no support");
            a->actorPosition=a->actorPreviousPosition=a->actorRenderPosition=start;a->actorYaw=direction*scene::kPi/4;scene::Vec3 expected=start,velocity{};auditKeys={GLFW_KEY_W};int compared=0;
            for(int i=0;i<125;++i){tick(*a);require(!a->actorNoclip&&!a->actorCollisionPreviewNoclip,"real-map fixture entered preview noclip");if(!a->actorGrounded)break;baselineGround(*a,expected,velocity);auditNear(a->actorPosition,expected,"real-map grounded position changed");auditNear(a->actorVelocity,velocity,"real-map grounded velocity changed");++compared;}
            totalCompared+=compared;std::cout<<"map route "<<start.x<<','<<start.y<<','<<start.z<<" direction="<<direction<<" grounded_baseline_ticks="<<compared<<" -> "<<a->actorPosition.x<<','<<a->actorPosition.y<<','<<a->actorPosition.z<<'\n';
        }
        require(totalCompared>=125,"insufficient real-map grounded baseline coverage");
    }
    std::cout<<"PASS production Source v1: grounded baseline/curb, no ramp magnet, release/landing, reverse momentum, held hops\n";ImGui::DestroyContext();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
