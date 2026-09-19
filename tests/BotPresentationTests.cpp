#include "gameplay/BotPresentation.h"
#include <iostream>
#include <stdexcept>
void require(bool value){if(!value)throw std::runtime_error("bot interpolation regression");}
int main(){
 for(float scale:{1.0f,.1f,.05f,.01f}){
  gameplay::bot::Actor bot;bot.position={100,0,0};bot.input.forward=1;bot.velocity.x=gameplay::iw::kRunSpeed;
  float last=0,minMove=1e9f,maxMove=0;int steps=0;
  for(int i=0;i<2000;++i){
   bot.fixedPresentationThisUpdate=false;gameplay::bot::step(bot,scale/120,1,false);
   const auto physical=bot.position;const auto rendered=gameplay::bot::presentationPosition(bot);
   require(scene::length(physical-bot.position)==0);
   if(i>150){const float move=rendered.x-last;require(move>0);minMove=std::min(minMove,move);maxMove=std::max(maxMove,move);++steps;}
   last=rendered.x;
  }
  require(maxMove/minMove<1.1f);
  require(bot.recoveryTime==0&&bot.routeDiversionTime==0);
  const auto held=gameplay::bot::presentationPosition(bot);gameplay::bot::step(bot,0,1,false);
  require(scene::length(gameplay::bot::presentationPosition(bot)-held)<.001f);
  bot.position={10000,10000,10000};require(scene::length(gameplay::bot::presentationPosition(bot)-bot.position)<.001f);
  bot.fixedPresentationThisUpdate=false;bot.position={2,3,4};require(scene::length(gameplay::bot::presentationPosition(bot)-bot.position)<.001f);
  std::cout<<"timescale "<<scale<<" continuous steps "<<steps<<" min/max "<<minMove<<" / "<<maxMove<<std::endl;
 }
 for(float scale:{1.f,.01f}){
  gameplay::bot::NavigationGraph graph;graph.blocks.push_back({{-1e6f,-1e6f,-1e6f},{1e6f,1e6f,1e6f}});
  gameplay::bot::Actor blocked;blocked.navigation=&graph;blocked.input.forward=1;
  for(int i=0;i<3600;++i)gameplay::bot::step(blocked,scale/120,1,false);
  require(blocked.recoveryTime>0);
 }
}
