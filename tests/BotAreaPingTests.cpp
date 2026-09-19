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
 p.advance(21,bots);CHECK(p.recipients.empty()&&p.remaining==0);
 p.set({1000,0,0},100,bots);p.set({2000,1,0},0,bots);CHECK(!p.contains(3));
 std::cout<<"Area ping selection, lifecycle, vertical arrival and replacement passed\n";
}
