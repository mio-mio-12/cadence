#include "gameplay/IwPresentation.h"
#include "gameplay/WorldActorPresentation.h"

#include <cmath>
#include <iostream>

namespace {
bool expect(bool value,const char* message){if(!value)std::cerr<<"FAILED: "<<message<<'\n';return value;}
}

int main(){
    int failures{};
    failures+=!expect(gameplay::presentation::falloffAlpha(5,5,.2f)==0,"mantle exit preserves its final pose");
    failures+=!expect(std::abs(gameplay::presentation::falloffAlpha(5.1,5,.2f)-.5f)<1e-5f,"mantle pose smoothly blends halfway through falloff");
    failures+=!expect(gameplay::presentation::falloffAlpha(5.3,5,.2f)==1,"mantle falloff completes at configured duration");
    const scene::Vec3 authoritative{10,20,30},interpolated{8,18,27};
    const auto rendered=gameplay::presentation::worldActorTransform(authoritative,interpolated,true,0,2);
    const auto recorded=gameplay::presentation::worldActorTransform(authoritative,interpolated,false,0,2);
    failures+=!expect(rendered.v[12]==8&&rendered.v[13]==18&&rendered.v[14]==29,"world body consumes interpolated position without trailing filter");
    failures+=!expect(recorded.v[12]==10&&recorded.v[13]==20&&recorded.v[14]==32,"recorded world body retains simulation coordinates");
    const auto frameStepped=gameplay::presentation::worldActorTransform(authoritative,authoritative,true,0,2);
    failures+=!expect(frameStepped.v[12]==10&&frameStepped.v[14]==32,"mantle and teleport current samples have no added lag");
    double clock=-1;
    failures+=!expect(gameplay::presentation::presentationElapsed(1,clock)==0,"first evaluation starts transition clock");
    failures+=!expect(std::abs(gameplay::presentation::presentationElapsed(1.01,clock)-.01f)<1e-6f,"transition uses elapsed simulation time");
    failures+=!expect(gameplay::presentation::presentationElapsed(1.01,clock)==0,"repeated render or recording evaluation cannot advance transition");
    failures+=!expect(gameplay::presentation::presentationElapsed(.5,clock)==0,"clock reset cannot reverse transitions");
    failures+=!expect(gameplay::iw::groundAcceleration(scene::Stance::Stand)==9.0f,"standing acceleration matches IW3");
    failures+=!expect(gameplay::iw::groundAcceleration(scene::Stance::Crouch)==12.0f,"crouched acceleration matches IW3");
    failures+=!expect(gameplay::iw::groundAcceleration(scene::Stance::Prone)==19.0f,"prone acceleration matches IW3");
    failures+=!expect(std::abs(gameplay::iw::viewHeight(scene::Stance::Stand)-152.4f)<0.001f&&std::abs(gameplay::iw::viewHeight(scene::Stance::Crouch)-101.6f)<0.001f&&std::abs(gameplay::iw::viewHeight(scene::Stance::Prone)-27.94f)<0.001f,"stance view heights convert IW inches to CAST centimetres");
    const auto standing=gameplay::iw::sampleViewBob(scene::kPi*0.5f,gameplay::iw::kRunSpeed,scene::Stance::Stand,false);
    failures+=!expect(std::abs(standing.horizontal-3.3782f)<0.001f,"standing horizontal bob scales with the gameplay world");
    const auto sprint=gameplay::iw::sampleViewBob(scene::kPi*0.5f,gameplay::iw::kRunSpeed*gameplay::iw::kSprintScale,scene::Stance::Stand,true);
    failures+=!expect(std::abs(sprint.horizontal-14.478f)<0.001f,"sprint horizontal bob scales with the gameplay world");
    failures+=!expect(gameplay::iw::landingDip(0.0f,0.3f,2.0f)==0.0f&&gameplay::iw::landingDip(0.15f,0.3f,2.0f)<0.0f&&gameplay::iw::landingDip(0.3f,0.3f,2.0f)==0.0f,"landing camera response returns to neutral");
    if(!failures)std::cout<<"All IW presentation tests passed.\n";return failures?1:0;
}
