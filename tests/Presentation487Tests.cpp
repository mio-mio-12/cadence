#include "gameplay/ScoreDisplay.h"
#include "gameplay/CameraShake.h"
#include "gameplay/LadderVelocity.h"
#include <cstdlib>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<"\n";return 1;}}while(false)
int main(){
    gameplay::score::Settings s;gameplay::score::State state;state.kill(s,1);CHECK(state.amount==100);state.kill(s,1.5);CHECK(state.amount==200);state.kill(s,3);CHECK(state.amount==100);state.kill(s,.1);CHECK(state.amount==100);
    CHECK(gameplay::score::sample(s,state,10).alpha==0);CHECK(gameplay::score::sample(s,state,.11).alpha==1);
    const auto p=std::filesystem::temp_directory_path()/"cadence_score487.score";CHECK(gameplay::score::save(s,p));gameplay::score::Settings loaded;CHECK(gameplay::score::load(loaded,p));CHECK(loaded.window==s.window&&loaded.font==s.font);std::filesystem::remove(p);
    gameplay::shake::Settings shake;CHECK(scene::length(gameplay::shake::sample(shake,1).position)==0);shake.enabled=true;const auto a=gameplay::shake::sample(shake,1),b=gameplay::shake::sample(shake,1);CHECK(scene::length(a.position-b.position)==0);
    const auto climb=gameplay::ladder::velocity({1,0,0},{0,1,0},1,0,{-1,0,0},508,false);CHECK(std::abs(climb.z-508)<.001f);CHECK(std::abs(climb.x)<.001f);
    const auto strafe=gameplay::ladder::velocity({1,0,0},{0,1,0},0,1,{-1,0,0},508,false);CHECK(std::abs(strafe.y-508)<.001f);
    const auto down=gameplay::ladder::velocity({1,0,0},{0,1,0},-1,0,{-1,0,0},508,false);CHECK(down.z<0);
    std::cout<<"PASS score stack/window/preset, deterministic shake, ladder-plane velocity\n";
}
