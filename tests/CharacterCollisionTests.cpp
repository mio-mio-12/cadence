#include "gameplay/CharacterCollision.h"
#include "gameplay/PlayerCollisionControls.h"
#include <sstream>
#include "gameplay/MantleAcquisition.h"
#include <iostream>
#include <limits>
using namespace scene;
static Mesh body(float low,float high,int variant=-1){Mesh m;m.actorVariant=variant;for(float z:{low,high,high}){Vertex v;v.position={0,0,z};m.vertices.push_back(v);}m.indices={0,1,2};return m;}
static void quad(glb::Map& map,Vec3 a,Vec3 b,Vec3 c,Vec3 d){for(auto p:{std::array{a,b,c},std::array{a,c,d}}){glb::CollisionTriangle t;t.a=p[0];t.b=p[1];t.c=p[2];t.normal=normalize(cross(t.b-t.a,t.c-t.a));t.minimum=t.maximum=t.a;for(auto v:p){t.minimum={std::min(t.minimum.x,v.x),std::min(t.minimum.y,v.y),std::min(t.minimum.z,v.z)};t.maximum={std::max(t.maximum.x,v.x),std::max(t.maximum.y,v.y),std::max(t.maximum.z,v.z)};}t.walkable=t.normal.z>.7f;t.blocking=!t.walkable;map.collision.push_back(t);}}
int main(){int fails=0;const auto check=[&](bool ok,const char* msg){if(!ok){++fails;std::cerr<<msg<<'\n';}};const auto near=[](float a,float b){return std::abs(a-b)<.001f;};
    check(near(gameplay::collision::playerStandingHeight(183,0),183),"auto clip default changed");
    check(near(gameplay::collision::playerStandingHeight(183,140),140),"clip override not used");
    check(near(gameplay::collision::playerStandingHeight(183,std::numeric_limits<float>::infinity()),183),"invalid clip did not fallback");
    check(near(gameplay::collision::stanceHeight(gameplay::collision::playerStandingHeight(183,140),Stance::Crouch,72,36,36),70),"clip override lost stance ratio");
    const auto preview=gameplay::collision::playerHeightPreviewCenter({10,20,800},0,30);
    check(near(preview.x,210)&&near(preview.y,20)&&near(preview.z,30),"clip preview not 2m ground anchored");
    for(const auto value:{"-5","nan","inf","1000001"}){float height=140;std::istringstream in(value);check(!gameplay::collision::readPlayerHeight(in,height)&&near(height,140),"invalid saved clip changed height");}
    CastScene actor;actor.bounds={{0,0,-10},{0,0,190},true};actor.meshes={body(-10,130,0),body(30,190,1)};
    auto attachment=body(-100,900);attachment.attachmentIndex=0;actor.meshes.push_back(attachment);auto weapon=body(0,1000);weapon.viewmodelWeapon=true;actor.meshes.push_back(weapon);
    gameplay::collision::CharacterHeightCache cache;
    check(near(cache.standingHeight(actor,0),140),"variant0 body-only height");
    check(near(cache.standingHeight(actor,1),200),"variant1 uses global renderer floor, not own floor");
    check(near(cache.standingHeight(actor,0,2,.635f),280.635f),"render scale and explicit floor lift");
    const auto builds=cache.rebuildCount();for(int i=0;i<100;++i)cache.standingHeight(actor,i%2,1+i*.01f);
    check(cache.rebuildCount()==builds,"repeated frame/scale queries rescan vertices");
    actor.meshes[0].modelTransform=translation({0,0,20});check(near(cache.standingHeight(actor,0),160),"mesh transform invalidates cache");
    actor.meshes[0].vertices[1].position.z=150;cache.invalidate();check(near(cache.standingHeight(actor,0),180),"explicit in-place edit invalidation");
    for(float x:{10.f,80.f})for(float y:{15.f,100.f})for(float sx:{-8.f,8.f})for(float sy:{-8.f,8.f}){
        const auto probe=gameplay::collision::scaleFootprintProbe({x,y},16.f,8.f,{sx,sy,0});
        check(near(probe.halfExtents.x,x*.5f)&&near(probe.halfExtents.y,y*.5f),"CSS quadrant uses half of each actual axis");
        check(near(std::abs(probe.offset.x)+probe.halfExtents.x,x)&&near(std::abs(probe.offset.y)+probe.halfExtents.y,y),"CSS quadrant stays inside actual footprint");
    }
    check(near(gameplay::collision::stanceHeight(180,Stance::Crouch),100),"crouch ratio");
    check(near(gameplay::collision::stanceHeight(180,Stance::Prone),50),"prone ratio");
    check(near(gameplay::collision::stanceHeight(180,Stance::Crouch,72,36,20),90),"Source duck ratio");
    check(near(gameplay::collision::stanceHeight(180,Stance::Stand),180),"stand restores stable model height");
    CastScene empty;check(near(cache.standingHeight(empty,-1,1,0,123),123),"empty model fallback");
    CastScene planar;planar.meshes={body(50,50)};check(near(cache.standingHeight(planar,-1,1,0,123),123),"planar model fallback");
    CastScene invalid;invalid.meshes={body(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN())};check(near(cache.standingHeight(invalid,-1,1,0,123),123),"nonfinite geometry fallback");
    check(near(cache.standingHeight(actor,0,std::numeric_limits<float>::quiet_NaN(),0,123),123),"nonfinite scale fallback");
    CastScene skinned;Bone b;b.restLocal.position={0,0,20};b.restGlobal=translation({0,0,20});b.inverseBind=translation({0,0,-10});skinned.skeleton.bones.push_back(b);skinned.meshes={body(0,100)};skinned.meshes[0].skinned=true;skinned.meshes[0].modelTransform=scale({1,1,2});
    check(near(cache.standingHeight(skinned),220),"rest skin palette then mesh transform order");
    CastScene helmetActor;helmetActor.meshes={body(0,140)};Bone head;head.name="j_head";head.restLocal.position={0,0,130};head.restGlobal=translation(head.restLocal.position);head.inverseBind=inverseAffine(head.restGlobal);helmetActor.skeleton.bones.push_back(head);
    Bone hand=head;hand.name="j_wrist_ri";helmetActor.skeleton.bones.push_back(hand);
    Attachment helmet;helmet.boneIndex=0;helmet.position={0,0,10};helmetActor.attachments.push_back(helmet);auto helmetMesh=body(0,15);helmetMesh.attachmentIndex=0;helmetMesh.modelTransform=translation({0,0,5});helmetActor.meshes.push_back(helmetMesh);
    Attachment held;held.boneIndex=1;helmetActor.attachments.push_back(held);auto heldMesh=body(0,900);heldMesh.attachmentIndex=1;helmetActor.meshes.push_back(heldMesh);
    check(near(cache.standingHeight(helmetActor),160),"head attachment uses bone local mesh transform and held prop excluded");
    helmetActor.attachments[0].position.z=20;check(near(cache.standingHeight(helmetActor),170),"head attachment edit invalidates cache");
    // Real swept collision: a 160-high doorway should admit the 140-high
    // model, not the previous 182.88 constant or a 200-high actor.
    glb::Map map;quad(map,{0,-100,160},{0,100,160},{200,100,160},{200,-100,160});
    quad(map,{0,-100,160},{0,-100,300},{0,100,300},{0,100,160});
    const auto sweep=[&](float h,bool rounded){return rounded?gameplay::mantle::sweepRounded(map,{-100,0,0},{100,0,0},scene::course::kPlayerRadius,h):gameplay::movement::sweep(map,{-100,0,0},{100,0,0},scene::course::kPlayerRadius,h);};
    for(bool rounded:{false,true}){check(sweep(140,rounded).fraction==1,"short model failed doorway clearance");check(sweep(gameplay::iw::kStandingActorHeight,rounded).fraction<1,"legacy tall hull did not hit ceiling");check(sweep(200,rounded).fraction<1,"oversized actor passed ceiling");check(sweep(gameplay::collision::stanceHeight(200,Stance::Crouch),rounded).fraction==1,"crouched actor failed clearance");}
    check(gameplay::movement::sweep(map,{100,0,0},{100,0,0},scene::course::kPlayerRadius,200).startSolid,"standing up box missed ceiling");
    check(gameplay::mantle::sweepRounded(map,{100,0,0},{100,0,0},scene::course::kPlayerRadius,200).startSolid,"standing up rounded missed ceiling");
    return fails?1:0;
}
