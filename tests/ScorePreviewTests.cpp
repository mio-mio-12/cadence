#include "gameplay/ScorePreview.h"
#include "render/MetalShader.h"
#include "app/FontBrowser.h"
#include <iostream>
#include <limits>
#include <sstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<'\n';return 1;}}while(false)
int main(){
    CHECK(cadence::fonts::matches("Segoe UI","seGOe"));CHECK(cadence::fonts::matches("consola.ttf",""));CHECK(!cadence::fonts::matches("Tahoma","consolas"));
    cadence::fonts::Browser browser;browser.hover("a.ttf",1);CHECK(browser.pendingPreview.empty());browser.hover("a.ttf",1.31);CHECK(browser.pendingPreview=="a.ttf");browser.pendingPreview.clear();browser.previewName="a.ttf";browser.hover("a.ttf",2);CHECK(browser.pendingPreview.empty());
    using namespace gameplay::score;Settings s;Preview preview;
    preview.advance(s,10);preview.advance(s,10.2);CHECK(std::abs(preview.time-.2f)<.0001f);
    const float saved=preview.time;preview.playing=false;preview.advance(s,10.4);CHECK(preview.time==saved);
    preview.restart();CHECK(preview.playing&&preview.time==0);preview.advance(s,11);
    const float end=duration(s);preview.time=end+preview.gap-.01f;preview.advance(s,11.02);CHECK(preview.time<.02f);
    preview.loop=false;preview.time=end-.01f;preview.advance(s,11.04);CHECK(!preview.playing&&preview.time==end);
    preview.useKillAmount=false;preview.amount=250;CHECK(preview.state(s).amount==250);preview.useKillAmount=true;CHECK(preview.state(s).amount==s.perKill);
    State live;live.kill(s,3);const auto amount=live.amount;preview.advance(s,12);CHECK(live.amount==amount&&live.last==3);
    s.anchor=4;CHECK(anchor(s,1280,720).x==1280&&anchor(s,1280,720).y==720);
    s.anchor=5;s.anchorX=.25;s.anchorY=.75;CHECK(anchor(s,1280,720).x==320&&anchor(s,1280,720).y==540);
    s.pivot=0;CHECK(pivot(s,{100,20},{60,12}).x==50);s.pivot=4;CHECK(pivot(s,{100,20},{60,12}).x==60);
    s.pivot=3;s.pivotX=.2f;s.pivotY=.9f;const auto point=pivot(s,{100,20},{60,12});CHECK(point.x==20&&point.y==18);
    const auto fixed=transform(point,{640,360},point,3,45);CHECK(fixed.x==640&&fixed.y==360);
    const auto rotated=transform({10,0},{100,50},{0,0},2,90);CHECK(std::abs(rotated.x-100)<.001f&&std::abs(rotated.y-70)<.001f);
    s.rotation=15;s.enterRotation=30;s.exitRotation=-45;State state;state.last=0;state.amount=100;
    CHECK(std::abs(sample(s,state,0).rotation-45)<.001f);CHECK(std::abs(sample(s,state,s.moveIn+.1f).rotation-15)<.001f);
    s.shadow=true;s.shadowDirection=123;s.shadowOffset=4;s.shadowSoftness=5;s.shadowAlpha=.4f;s.glowDirection=234;s.glowOffset=2;s.glowSoftness=7;
    const auto path=std::filesystem::temp_directory_path()/"cadence_score491_test.score";CHECK(save(s,path));Settings loaded;CHECK(load(loaded,path));CHECK(loaded.rotation==15&&loaded.pivot==3&&loaded.anchor==5);CHECK(effectFields(loaded)==effectFields(s));
    {std::ofstream out(path);out<<"CADENCESCORE 2\n";std::apply([&](auto&...v){((out<<v<<' '),...);},fields(s));out<<std::quoted(s.font)<<'\n';std::apply([&](auto&...v){((out<<v<<' '),...);},transformFields(s));}
    CHECK(load(loaded,path)&&!loaded.shadow&&loaded.glowOffset==0&&loaded.glowSoftness==3&&loaded.pivot==3);
    // Version 1 keeps its original top-left pivot and screen-center anchor.
    {std::ofstream out(path);out<<"CADENCESCORE 1\n";std::apply([&](auto&...v){((out<<v<<' '),...);},fields(s));out<<std::quoted(s.font)<<'\n';}
    CHECK(load(loaded,path)&&loaded.pivot==1&&loaded.anchor==0&&loaded.rotation==0);
    {std::ofstream out(path);out<<"CADENCESCORE 2\n";}CHECK(!load(loaded,path));CHECK(loaded.pivot==1);
    std::filesystem::remove(path);
    for(float radius:{0.f,2.f,6.f,64.f}){const auto taps=effectKernel(radius);float sum{},x{},y{};CHECK(taps.size()<=625);for(const auto& tap:taps){CHECK(tap.weight>0&&std::isfinite(tap.weight));sum+=tap.weight;x+=tap.x*tap.weight;y+=tap.y*tap.weight;}CHECK(std::abs(sum-1)<.0001f&&std::abs(x)<.0001f&&std::abs(y)<.0001f);}
    render::metal::Settings metal;metal.coverage=-10;metal.reflection=std::numeric_limits<float>::quiet_NaN();metal.mode=9;metal.sanitize();CHECK(metal.coverage==0&&metal.reflection==1&&metal.mode==2);
    std::cout<<"PASS score loop/pause/scrub clock, isolated preview, anchors, pivots, rotation, v1/v2 presets, metal sanitization\n";
}
