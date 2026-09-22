#include "render/Water.h"
#include <sstream>
#include <iostream>
#include <limits>
int main(){
    int failures=0;const auto check=[&](bool ok,const char* what){if(!ok){++failures;std::cerr<<what<<'\n';}};
    render::water::Settings s;s.height=123;s.enabled=true;s.waveHeight=std::numeric_limits<float>::quiet_NaN();
    s.radius=std::numeric_limits<float>::infinity();s.quality=99;s.sanitize();
    check(std::isfinite(s.waveHeight)&&s.radius==250&&s.quality==2,"invalid settings sanitized");
    {auto manual=s;manual.radius=5000;manual.waveHeight=25;manual.roughness=2;manual.reflection=12;
    manual.timeOffset=-100000;manual.seed=-32;manual.surface.foamDetail=3;manual.optics.crestLight=8;
    manual.sanitize();check(manual.radius==5000&&manual.waveHeight==25&&manual.roughness==2&&manual.reflection==12&&manual.timeOffset==-100000&&manual.seed==-32&&manual.surface.foamDetail==3&&manual.optics.crestLight==8,"manual finite overrides preserved");
    std::stringstream saved;saved<<static_cast<render::water::Appearance&>(manual);render::water::Appearance restored;saved>>restored;
    check(restored.waveHeight==25&&restored.reflection==12&&restored.roughness==2,"manual overrides survive preset reload");
    manual.wavelength=0;manual.swellLength=0;for(const auto& wave:render::water::waves(manual))check(std::isfinite(wave.k)&&std::isfinite(wave.omega),"zero wavelength is guarded locally");}
    std::stringstream text;text<<render::water::preset(2);
    text>>static_cast<render::water::Appearance&>(s);
    check(s.enabled&&s.height==123&&s.rain==.8f,"appearance presets preserve placement");
    const auto previous=s.rain;std::stringstream invalid("1 2 3");
    invalid>>static_cast<render::water::Appearance&>(s);
    check(invalid.fail()&&s.rain==previous,"truncated appearance is transactional");
    render::water::Optics optical;optical.absorption=8;optical.shoreWidth=2;
    std::stringstream optics;optics<<optical;render::water::Optics restored;optics>>restored;
    check(restored.absorption==8&&restored.shoreWidth==2,"optics preset roundtrip");
    std::stringstream broken("1 .2");broken>>restored;
    check(broken.fail()&&restored.absorption==8,"optics read transactional");
    for(int p=0;p<5;++p){
        const auto a=render::water::preset(p);const auto w=render::water::waves(a);
        const float q=render::water::safeChop(w,a.choppiness);float bound=0;
        for(const auto& v:w)bound+=v.amplitude;
        for(int angle=0;angle<360;angle+=5){
            const float x=std::cos(angle*scene::kPi/180),y=std::sin(angle*scene::kPi/180);float compression=0;
            for(const auto& v:w)compression+=v.amplitude*v.k*q*std::pow(v.x*x+v.y*y,2);
            check(compression<.97001f,"directional compression prevents folding");
        }
        check(render::water::safeChop(w,8)>q||a.choppiness==8,"choppiness continues increasing");
        for(float t:{0.f,.125f,1.f,300.f,-30.f}){
            const auto d=render::water::displacement(w,q,1200,900,t,a.surface.tightness),again=render::water::displacement(w,q,1200,900,t,a.surface.tightness);
            check(std::isfinite(d.x)&&std::isfinite(d.y)&&std::isfinite(d.z),"finite surface");
            check(d.x==again.x&&d.y==again.y&&d.z==again.z,"stateless seek determinism");
            check(std::abs(d.z)<=bound*render::water::crestProfile(scene::kPi*.5,a.surface.tightness)[0]+.001f,"height bound");
        }
    }
    s.waveHeight=0;const auto w=render::water::waves(s);
    const auto flat=render::water::displacement(w,2,0,0,3);
    check(flat.x==0&&flat.y==0&&flat.z==0,"zero waves is exactly flat");
    auto a=render::water::preset(3),b=a;b.seed+=1;
    const auto wa=render::water::waves(a),wb=render::water::waves(b);
    check(wa[0].phase!=wb[0].phase&&wa[0].k!=wb[0].k,"seed varies phases and wavelengths");
    b=a;b.crossSwell=0;const auto single=render::water::waves(b);
    check(single[12].amplitude==0&&wa[12].amplitude>0,"independent cross swell");
    for(int h=0;h<=12;++h)for(float length:{1.f,18.f,100.f})for(float spread:{0.f,.5f,1.f}){
        a.waveHeight=float(h);a.wavelength=length;a.spread=spread;
        const auto ws=render::water::waves(a);float xx=0,xy=0,yy=0;const float q=render::water::safeChop(ws,8);
        for(const auto& v:ws){const float s=v.amplitude*v.k*q;xx+=s*v.x*v.x;xy+=s*v.x*v.y;yy+=s*v.y*v.y;}
        check(.5f*(xx+yy+std::sqrt((xx-yy)*(xx-yy)+4*xy*xy))<.97001f,"extreme settings remain nonfolding");
    }
    using namespace render::water;
    Surface sf;sf.tightness=.91f;sf.foamPersistence=3;
    std::stringstream st;st<<sf;Surface sr;st>>sr;
    check(sr.tightness==sf.tightness&&sr.foamPersistence==3,"surface preset roundtrip");
    std::stringstream badSurface(".5 1");badSurface>>sr;
    check(badSurface.fail()&&sr.tightness==sf.tightness,"surface preset transactional");
    // Width above the top 10% of a wave, independent of height normalization.
    int widths[2]{};
    for(int shape=0;shape<2;++shape){
        const float tight=float(shape),peak=crestProfile(scene::kPi*.5,tight)[0];
        const float trough=crestProfile(-scene::kPi*.5,tight)[0];
        double mean=0,variance=0;
        for(int k=0;k<8192;++k){
            const double phase=(k+.5)*6.283185307179586/8192;
            const auto p=crestProfile(phase,tight);
            mean+=p[0];variance+=p[0]*p[0];
            widths[shape]+=p[0]>trough+.9f*(peak-trough);
            check(std::isfinite(p[0])&&std::isfinite(p[1]),"finite tight crest");
            const auto repeat=crestProfile(phase+6.283185307179586,tight);
            check(std::abs(p[0]-repeat[0])<1.e-5f,"periodic profile seam");
            const float numeric=(crestProfile(phase+1.e-4,tight)[0]-crestProfile(phase-1.e-4,tight)[0])/.0002f;
            check(std::abs(numeric-p[1])<.006f,"analytic crest slope");
        }
        check(std::abs(mean/8192)<.0001&&std::abs(variance/8192-.5)<.0001,"zero mean and preserved wave variance");
    }
    check(widths[1]<widths[0]*.2f,"tight crest top less than a fifth of sine width");
    for(int k=0;k<32;++k){
        const float u=k*.713f,v=k*.321f;
        const auto f=foamCell(u,v,8),g=foamCell(u+1,v-2,8);
        check(std::abs(f[0]-g[0])<.0002f,"periodic bubble texture");
    }
    const auto tex=foamTexture(),same=foamTexture();
    check(tex==same&&tex.size()==foamTextureSize*foamTextureSize*4,"deterministic bubble texture");
    int walls=0,interiors=0;for(std::size_t k=0;k<tex.size();k+=4){walls+=tex[k]<64;interiors+=tex[k]>192;}
    check(walls>1000&&interiors>1000,"foam has both thin walls and bubble interiors");
    std::cout<<"crest width samples sine="<<widths[0]<<" tight="<<widths[1]<<'\n';
    std::cout<<(failures?"FAILED":"PASS")<<" water settings, presets and displacement\n";return failures?1:0;
}
