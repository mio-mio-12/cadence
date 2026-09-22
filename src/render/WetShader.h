#pragma once
namespace render {
// Independent procedural surface-water implementation. No borrowed game shader
// or textures. Material coordinates stay on the mesh through skeletal motion.
inline constexpr const char* kWetShader=R"GLSL(
uniform bool uWetGround,uWetSurface,uWetShelter;
uniform sampler2D uRainShelter;
uniform vec3 uWetTile,uWetWind,uWetCenter;
uniform vec4 uWetA,uWetB,uWetC;
uniform float uWetTime,uWetCoat;
uniform vec4 uWetDetail,uWetMaterial,uWetEdge;
uniform bool uWetCoatAll;
in vec3 vWeatherPosition;
in vec3 vWeatherNormal;
vec2 wetHash(vec2 p){uvec2 h=uvec2(ivec2(floor(p)));h=h*1664525u+1013904223u;h.x+=h.y*1664525u;h.y+=h.x*1664525u;h^=h>>16u;h*=2246822519u;h^=h>>13u;return vec2(h&0x00ffffffu)/16777216.;}
float wetNoise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*f*(f*(f*6.-15.)+10.);return mix(mix(wetHash(i).x,wetHash(i+vec2(1,0)).x,f.x),mix(wetHash(i+vec2(0,1)).x,wetHash(i+1.).x,f.x),f.y);}
float wetFilteredNoise(vec2 p){float footprint=max(length(dFdx(p)),length(dFdy(p)));return mix(wetNoise(p),.5,smoothstep(.35,1.5,footprint));}
float wetPatch(vec2 p){vec2 warp=vec2(wetNoise(p*.53),wetNoise(p*.53+31.));p+=warp*1.4;return wetNoise(p)*.57+wetNoise(p*2.03+7.)*.28+wetNoise(p*4.17-9.)*.15;}
float wetBead(vec2 p){
    vec2 id=floor(p),r=wetHash(id);
    float life=uWetTime*.31+r.x*15.,age=fract(life);
    r=wetHash(id+vec2(floor(life)*19.,floor(life)*43.));
    vec2 q=fract(p)-(.5+(r-.5)*uWetDetail.x);
    float active=smoothstep(0.,.09,age)*(1.-smoothstep(.68,1.,age));
    q.x+=.025*sin(q.y*13.+r.y*21.);float rad=mix(.07,.19,r.y);
    float d=length(q/vec2(rad,rad*mix(.8,1.25,r.x)));
    return pow(max(0.,1.-d*d),2.)*active*step(r.x,clamp(uWetC.x,0.,1.));
}
float wetDrip(vec2 p){
    // Irregular independently paced rivulets with rounded heads and thin tails.
    float col=floor(p.x),r=wetHash(vec2(col,19.)).x;
    float y=p.y+uWetTime*uWetC.w*mix(.35,.9,r);
    vec2 cell=vec2(col,floor(y));vec2 h=wetHash(cell);
    float meander=wetNoise(vec2(p.y*uWetDetail.z+col*7.,uWetTime*uWetC.w*.19))-.5;
    float x=fract(p.x)-(.25+h.x*.5)+meander*uWetDetail.y+.035*sin(p.y*6.+col*17.);
    float tail=fract(y),width=mix(.025,.08,h.y)*(1.-tail*.8);
    float stripe=exp(-x*x/max(.00001,width*width))*smoothstep(0.,.06,tail)*(1.-smoothstep(.3,.85,tail));
    return stripe*step(h.y,clamp(uWetC.z,0.,1.));
}
float wetHeight(vec3 p,vec3 n,float puddle){
    float size=max(.04,abs(uWetC.y));vec3 q=p/size;
    vec3 w=pow(abs(n),vec3(4));w/=max(.001,w.x+w.y+w.z);
    float beads=wetBead(q.yz)*w.x+wetBead(q.xz+13.)*w.y+wetBead(q.xy+37.)*w.z;
    float drips=wetDrip(q.yz*vec2(.45,.08))*w.x+wetDrip(q.xz*vec2(.45,.08)+11.)*w.y;
    vec2 r=p.xy/max(1.,size*24.),id=floor(r),delta=fract(r)-(.2+.6*wetHash(id));
    float age=fract(uWetTime*.83+wetHash(id+4.).x*13.);
    float dist=length(delta),ring=sin((dist-age*.65)*48.)*exp(-abs(dist-age*.65)*32.)*(1.-age);
    return (uWetSurface?(beads*.1+drips*.08)*(1.-puddle):0.)+ring*uWetB.w*puddle*.035;
}
void surfaceWater(inout vec3 n,inout vec3 rgb,out float wet,out float puddle){
    wet=0.;puddle=0.;if(!uWetGround&&!uWetSurface)return;
    float exposure=1.;
    if(uWetShelter){vec2 lane=vWorldPosition.xy+uWetWind.xy*vWorldPosition.z;
        vec2 uv=(lane-uWetTile.xy)/(2.*uWetTile.z)+.5;exposure=0.;
        if(all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)))){
            vec4 hit=texture(uRainShelter,uv);vec2 texelUv=(floor(uv*vec2(textureSize(uRainShelter,0)))+.5)/vec2(textureSize(uRainShelter,0));
            vec2 sampleLane=(texelUv-.5)*(2.*uWetTile.z)+uWetTile.xy;
            float denom=hit.w-dot(hit.yz,uWetWind.xy);
            float roof=hit.x;if(abs(denom)>.0001&&hit.x> -1e7)roof-=dot(hit.yz,lane-sampleLane)/denom;
            exposure=smoothstep(-6.,-1.,vWorldPosition.z-roof);
            // Fade around the continuously moving camera, never the snapped
            // atlas center. Its complete support stays inside adjacent tiles.
            float range=length(lane-uWetCenter.xy)/max(1.,uWetCenter.z);
            exposure*=1.-smoothstep(.65,1.15,range);}}
    float upward=smoothstep(.65,.96,normalize(vNormal).z);
    if(uWetGround){float pattern=wetPatch(vWorldPosition.xy/max(.1,abs(uWetB.x)));float edge=max(.002,abs(uWetB.y));
        vec2 detailUv=vWorldPosition.xy/max(.1,abs(uWetB.x))*uWetEdge.y;
        detailUv=mat2(.8,-.6,.6,.8)*detailUv;
        pattern+=((wetFilteredNoise(detailUv)*.7+wetFilteredNoise(detailUv*2.03+17.)*.3)-.5)*uWetEdge.x;
        edge=max(edge,fwidth(pattern)*.75);
        puddle=smoothstep(1.-uWetA.w-edge,1.-uWetA.w+edge,pattern)*upward*exposure;
        wet=max(puddle,uWetA.x*upward*exposure);}
    if(uWetSurface)wet=max(wet,uWetA.x*exposure);
    wet=clamp(wet,0.,1.);rgb*=max(0.,1.-uWetA.y*wet);
    vec3 p=uWetGround?vWorldPosition:vWeatherPosition;
    float height=wetHeight(p,normalize(uWetGround?vNormal:vWeatherNormal),puddle);
    vec3 dx=dFdx(vWorldPosition),dy=dFdy(vWorldPosition),r1=cross(dy,n),r2=cross(n,dx);
    float det=dot(dx,r1);vec3 grad=(r1*dFdx(height)+r2*dFdy(height))/max(1e-8,abs(det))*sign(det);
    float footprint=max(length(dFdx(p)),length(dFdy(p)))/max(.04,abs(uWetC.y));
    float aa=1.-smoothstep(.35,1.5,footprint);
    n=normalize(mix(n,normalize(vNormal),puddle*.9)-grad*wet*aa*uWetDetail.w);
    if(!uWetCoatAll)wet*=max(puddle,smoothstep(.001,.04,height));
}
)GLSL";
}
