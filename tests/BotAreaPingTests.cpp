#include "gameplay/BotAreaPing.h"
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<__LINE__<<" "<<#x;std::abort();}}while(false)
int main(){
 std::vector<gameplay::bot::Actor> bots(5);for(size_t i=0;i<bots.size();++i){bots[i].id=static_cast<unsigned>(i+1);bots[i].alive=true;}
 gameplay::bot::AreaPing p;p.set({1000,0,0},0,bots);CHECK(p.recipients.empty());
 p.set({1000,0,0},50,bots);CHECK(p.recipients.size()==3);auto chosen=p.recipients;std::sort(chosen.begin(),chosen.end());CHECK(std::adjacent_find(chosen.begin(),chosen.end())==chosen.end());
 p.set({1000,0,0},100,bots);CHECK(p.recipients.size()==5);
 bots[0].alive=false;p.advance(.1f,bots);CHECK(!p.contains(1));bots[0].alive=true;p.advance(.1f,bots);CHECK(!p.contains(1));
 bots[1].position={999,0,0};p.advance(.1f,bots);CHECK(!p.contains(2));
 bots[2].position={999,0,300};p.advance(.1f,bots);CHECK(p.contains(3));
 p.advance(21,bots);CHECK(p.contains(3));
 p.advance(301,bots);CHECK(p.recipients.empty()&&p.remaining==0);
 p.set({1000,0,0},100,bots);p.set({2000,1,0},0,bots);CHECK(!p.contains(3));
 bots.resize(18);for(size_t i=0;i<bots.size();++i){bots[i].id=unsigned(i+1);bots[i].alive=true;bots[i].position={0,-8000,0};}
 p.set({0,0,0},100,bots);CHECK(p.arrivalRadius>=300&&p.remaining>=140);
 bots[0].position={0,200,0};bots[1].position={0,200,200};p.advance(.1f,bots);
 CHECK(!p.contains(1)&&p.contains(2));
 bots[2].position={0,0,0};bots[2].mantling=true;p.advance(.1f,bots);CHECK(p.contains(3));
 bots[2].mantling=false;bots[2].grounded=false;p.advance(.1f,bots);CHECK(p.contains(3));
 bots[2].grounded=true;p.advance(.1f,bots);CHECK(!p.contains(3));
 std::cout<<"Area ping selection, lifecycle, vertical arrival and replacement passed\n";
}
