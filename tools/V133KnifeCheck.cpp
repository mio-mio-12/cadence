#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";std::exit(1);}}while(false)
int main(){
    auto state=std::make_unique<AppState>();auto& a=*state;a.actorMode=true;a.actorPosition={};a.actorViewHeight=160;a.botHeadHeight=180;
    a.weaponProfile.meleeWeapon=true;a.botActorScene.emplace();a.bots.resize(4);
    const scene::Vec3 positions[]={{100,0,0},{100,80,0},{-100,0,0},{200,0,0}};
    for(int i=0;i<4;++i){a.bots[i].position=positions[i];a.bots[i].alive=true;a.bots[i].health=100;}
    ++a.playerKnifeAttack;processPlayerKnifeAttack(a);
    CHECK(!a.bots[0].alive&&a.bots[0].health==0&&a.bots[0].respawnTime==a.botRespawnTime);
    CHECK(a.bots[1].alive&&a.bots[2].alive&&a.bots[3].alive);CHECK(a.recoilSequence==0&&a.hitmarkerKind==3);
    a.bots[0].alive=true;a.bots[0].health=100;processPlayerKnifeAttack(a);CHECK(a.bots[0].alive);
    a.takePreview=true;++a.playerKnifeAttack;processPlayerKnifeAttack(a);CHECK(a.bots[0].alive);
    a.takePreview=false;processPlayerKnifeAttack(a);CHECK(a.bots[0].alive);
    a.weaponProfile.meleeWeapon=false;++a.playerKnifeAttack;processPlayerKnifeAttack(a);CHECK(a.bots[0].alive);
    a.weaponProfile.meleeWeapon=true;++a.playerKnifeAttack;processPlayerKnifeAttack(a);CHECK(!a.bots[0].alive);
    CHECK(a.playerMuzzleFlashTime==0&&a.recoilSequence==0);
    a.loadedMap.emplace();
    const scene::Vec3 wall[]={{50,-100,0},{50,100,0},{50,100,300},{50,-100,300}};
    for(const auto indices:{std::array<int,3>{0,1,2},std::array<int,3>{0,2,3}}){
        scene::glb::CollisionTriangle triangle;triangle.a=wall[indices[0]];triangle.b=wall[indices[1]];triangle.c=wall[indices[2]];
        triangle.normal={-1,0,0};triangle.minimum={50,-100,0};triangle.maximum={50,100,300};triangle.blocking=true;a.loadedMap->collision.push_back(triangle);
    }
    a.loadedMap->buildCollisionIndex();a.bots[0].alive=true;a.bots[0].health=100;++a.playerKnifeAttack;processPlayerKnifeAttack(a);CHECK(a.bots[0].alive);
    a.loadedMap->collision.clear();a.loadedMap->buildCollisionIndex();++a.playerKnifeAttack;processPlayerKnifeAttack(a);CHECK(!a.bots[0].alive);
    std::cout<<"PASS actual map collision wall blocks knife; empty collision allows it\n";
    std::cout<<"PASS actual knife combat: centered close target killed; edge/behind/distant excluded; one check per swing; replay/gun guards; respawn initialized; no gunshot emission\n";
}
