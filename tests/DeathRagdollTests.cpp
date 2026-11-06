#include "gameplay/DeathRagdoll.h"
#include "take/Take.h"
#include "app/BotDeathPolicy.h"
#include <iostream>
#ifdef CADENCE_TEST_WEAPON_DROP
#include "gameplay/WeaponDrop.h"
#endif
int main(){
    int failures=0;const auto check=[&](bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';++failures;}};
    scene::Skeleton skeleton;std::vector<scene::Mat4> pose;
    const char* names[]={"pelvis","spine_0","head_0","arm_upper_L","arm_lower_L","hand_L","arm_upper_R","arm_lower_R","hand_R","leg_upper_L","leg_lower_L","ankle_L","leg_upper_R","leg_lower_R","ankle_R"};
    const scene::Vec3 positions[]={{0,0,100},{0,0,130},{0,0,165},{0,20,140},{10,30,115},{20,35,95},{0,-20,140},{10,-30,115},{20,-35,95},{0,10,95},{8,10,55},{0,10,10},{0,-10,95},{8,-10,55},{0,-10,10}};
    for(int i=0;i<15;++i){scene::Bone bone;bone.name=names[i];bone.parent=i?0:-1;skeleton.boneByName[bone.name]=i;skeleton.bones.push_back(bone);pose.push_back(scene::translation(positions[i])*scene::scale({1.2f,1.2f,1.2f}));}
    gameplay::ragdoll::State rag;gameplay::ragdoll::Settings config;config.enabled=true;
    check(rag.begin(skeleton,pose,config),"complete anatomy initializes");auto initial=pose;rag.apply(pose);
    for(size_t i=0;i<pose.size();++i)check(pose[i].v==initial[i].v,"handoff is exact");
    const auto floor=[](scene::Vec3,scene::Vec3 to,float r){to.z=std::max(r,to.z);return to;};
    take::Take recording;recording.boneCount=1;recording.botCount=1;recording.botBoneCount=pose.size();recording.actor.baseModel="view.cast";recording.botActor.baseModel="body.cast";
    for(int frame=0;frame<240;++frame){rag.advance(1.f/60,floor);rag.apply(pose);
        for(const auto& m:pose){for(float f:m.v)check(std::isfinite(f),"finite matrices");check(std::abs(scene::length({m.v[0],m.v[1],m.v[2]})-1.2f)<.001f,"bone scale preserved");}
        take::Sample sample;sample.time=frame/60.f;sample.pose={scene::Mat4::identity()};take::RecordedActorState bot;bot.alive=false;bot.pose=pose;sample.bots.push_back(bot);recording.samples.push_back(sample);
    }
    check(rag.points[0].position.z<90,"gravity moves corpse");for(const auto& p:rag.points)check(p.position.z>=config.radius-.01f,"floor collision");
    const auto before=pose;rag.advance(0,floor);rag.apply(pose);check(before[0].v==pose[0].v,"pause does not simulate");
    for(size_t i=100;i<recording.samples.size();++i)recording.samples[i].corpses.push_back({55,0,float(recording.samples.size()-i)/recording.samples.size(),recording.samples[i].bots[0].pose});
    std::string error;const auto path=std::filesystem::temp_directory_path()/"cadence_ragdoll_test.c_dm";
    check(take::save(recording,path,error),"recording saves");take::Take replay;check(take::load(path,replay,error),"recording reloads");std::error_code ec;std::filesystem::remove(path,ec);
    if(replay.samples.size()==recording.samples.size())for(size_t i=0;i<replay.samples.size();++i)for(size_t b=0;b<pose.size();++b)check(replay.samples[i].bots[0].pose[b].v==recording.samples[i].bots[0].pose[b].v,"ragdoll recorded exactly");else check(false,"sample count");
    const auto a=replay.interpolatedSample(1.234f),b=replay.interpolatedSample(.2f),c=replay.interpolatedSample(1.234f);check(a.bots[0].pose[0].v==c.bots[0].pose[0].v,"scrubbing independent of simulation");
    check(replay.samples[120].corpses.size()==1&&replay.samples[120].corpses[0].id==55&&replay.samples[120].corpses[0].pose[0].v==recording.samples[120].corpses[0].pose[0].v,"persistent bodies survive save/load");
    const auto fading=replay.interpolatedSample(2.005f);check(fading.corpses.size()==1&&fading.corpses[0].opacity<replay.samples[120].corpses[0].opacity,"recorded dissolve interpolates");
    gameplay::ragdoll::Bodies bodies;bodies.keep=true;bodies.maximum=1;bodies.fadeSeconds=1;
    bodies.retain(gameplay::ragdoll::State(rag),pose);bodies.retain(gameplay::ragdoll::State(rag),pose);bodies.advance(.5f,floor);
    check(bodies.bodies.size()==2&&bodies.bodies[0].opacity==.5f&&bodies.bodies[1].opacity==1,"oldest retained body fades at cap");bodies.advance(.5f,floor);check(bodies.bodies.size()==1,"faded body removed");
    config.gravity=-1;config.settleSeconds=0;gameplay::ragdoll::State rising;check(rising.begin(skeleton,initial,config),"negative settings accepted");for(int i=0;i<600;++i)rising.advance(1.f/60,floor);check(rising.elapsed>9&&rising.points[0].position.z>100,"negative gravity and unlimited duration applied");
    config.gravity=0;config.damping=0;config.movementMomentum=.5f;config.shotImpulse=120;
    gameplay::ragdoll::State launch;check(launch.begin(skeleton,initial,config,{240,0,0},{0,-1,0}),"directional forces accepted");
    const auto origin=launch.points[0].position;
    launch.advance(1.f/30,[](scene::Vec3,scene::Vec3 p,float){return p;});
    check(launch.points[0].position.x>origin.x+3&&launch.points[0].position.y<origin.y-3,"movement and impact push along independent directions");
    recording.samples.front().bots.front().detached={{2,scene::translation({10,0,0})}};
    recording.samples.back().bots.front().detached={{2,scene::translation({90,0,0})}};
    check(take::save(recording,path,error)&&take::load(path,replay,error),"detached weapon take round trip");
    if(!replay.samples.empty()&&!replay.samples.front().bots.front().detached.empty())
        check(replay.samples.front().bots.front().detached.front().mount.v[12]==10,"detached mount preserved");
    else check(false,"detached mount present");
    std::filesystem::remove(path);
    gameplay::ragdoll::State invalid;check(!invalid.begin({},pose,config),"unsupported anatomy rejected safely");
    scene::CastScene pool;scene::Animation generic;generic.action=scene::ActionRole::Death;generic.domain=scene::AnimationDomain::PlayerBody;generic.sourceName="pb_generic_stand_idle_death_clip123.cast";generic.tracks.resize(1);pool.animations.push_back(generic);check(cadence::prepareBotDeaths(pool).empty(),"generic static death cannot enter combat pool");
#ifdef CADENCE_TEST_WEAPON_DROP
    const auto runDrop=[&](const scene::glb::Map* map,float bounce){
        gameplay::ragdoll::WeaponDropSettings settings;settings.bounce=bounce;
        gameplay::ragdoll::DroppedWeapon drop;
        drop.world=std::make_shared<gameplay::ragdoll::DropWorld>(map);
        drop.id=drop.world->add({0,0,120},{},{20,6,4},{80,0,0},{},settings);
        drop.local={{2,scene::Mat4::identity()}};
        float rebound=0;bool touched=false;
        for(int i=0;i<240;++i){
            drop.world->step(1.f/120);
            const auto mount=drop.pose().front().mount;
            touched|=mount.v[14]<9;
            if(touched)rebound=std::max(rebound,mount.v[14]);
        }
        const auto landed=drop.pose().front().mount;
        check(touched&&landed.v[14]>1&&landed.v[12]>10,"dropped weapon moves and collides");
        return rebound;
    };
    const auto flat=runDrop(nullptr,0.f),bounced=runDrop(nullptr,.8f);
    check(bounced>flat+15.f,"Box3D restitution changes rebound");
    scene::glb::Map testMap;
    scene::glb::CollisionTriangle floorTriangle;
    floorTriangle.a={-500,-500,0};floorTriangle.b={500,-500,0};floorTriangle.c={0,500,0};
    floorTriangle.minimum={-500,-500,0};floorTriangle.maximum={500,500,0};
    floorTriangle.walkable=true;floorTriangle.normal={0,0,1};
    testMap.collision.push_back(floorTriangle);
    testMap.buildCollisionIndex();
    runDrop(&testMap,0.f);
    scene::glb::Map separated;
    auto landing=floorTriangle;
    landing.a={650,-180,0};landing.b={980,-180,0};landing.c={800,180,0};
    landing.minimum={650,-180,0};landing.maximum={980,180,0};
    auto start=floorTriangle;
    start.a={0,-200,-900};start.b={80,-200,-900};start.c={0,200,-900};
    start.minimum={0,-200,-900};start.maximum={80,200,-900};
    separated.collision={start,landing};
    separated.collisionCellSize=256.f;
    separated.buildCollisionIndex();
    gameplay::ragdoll::DroppedWeapon traveling;
    traveling.world=std::make_shared<gameplay::ragdoll::DropWorld>(&separated);
    traveling.id=traveling.world->add({20,0,500},{},{20,6,4},{940,0,0},{},{});
    traveling.local={{0,scene::Mat4::identity()}};
    float lowest=500.f;
    for(int i=0;i<135;++i){traveling.world->step(1.f/120);lowest=std::min(lowest,traveling.pose().front().mount.v[14]);}
    check(lowest>=1.f&&lowest<20.f,"fast dropped weapon collides outside its initial collision cells");
#endif
    return failures?1:0;
}
