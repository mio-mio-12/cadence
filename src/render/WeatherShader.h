#pragma once
#include <string>
namespace render {
// Original procedural shaders; no third-party shader or texture assets.
inline const std::string kWeatherNoise=R"GLSL(
float whash(vec3 p){p=fract(p*.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
float wnoise(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(mix(whash(i),whash(i+vec3(1,0,0)),f.x),mix(whash(i+vec3(0,1,0)),whash(i+vec3(1,1,0)),f.x),f.y),mix(mix(whash(i+vec3(0,0,1)),whash(i+vec3(1,0,1)),f.x),mix(whash(i+vec3(0,1,1)),whash(i+vec3(1)),f.x),f.y),f.z);}
float wfbm(vec3 p){return .57143*wnoise(p)+.28571*wnoise(p*2.03+19.)+.14286*wnoise(p*4.11-7.);}
vec3 wfinite(vec3 c){return vec3(isnan(c.x)||isinf(c.x)?0.:clamp(c.x,0.,60000.),isnan(c.y)||isinf(c.y)?0.:clamp(c.y,0.,60000.),isnan(c.z)||isinf(c.z)?0.:clamp(c.z,0.,60000.));}
)GLSL";
inline const std::string kWeatherCloud=kWeatherNoise+R"GLSL(
uniform bool uWeatherCloud;
uniform float uWeatherCloudStrength;
uniform vec4 uWeatherCloudShape;
uniform vec4 uWeatherCloudMotion;
uniform vec3 uWeatherCloudSun;
float weatherCloud(vec3 world){
    if(!uWeatherCloud)return 1.;
    vec3 sun=uWeatherCloudSun;
    vec2 projected=world.xy+sun.xy*((uWeatherCloudMotion.z-world.z)/max(.1,sun.z));
    vec3 q=vec3((projected-uWeatherCloudMotion.xy)/max(1.,uWeatherCloudShape.x),uWeatherCloudMotion.w);
    float n=mix(wnoise(q),wfbm(q),uWeatherCloudShape.w);
    float cover=smoothstep(1.-uWeatherCloudShape.y-uWeatherCloudShape.z,1.-uWeatherCloudShape.y+uWeatherCloudShape.z,n);
    return 1.-cover*clamp(uWeatherCloudSun.z<0.?0.:uWeatherCloudSun.z*10.,0.,1.);
}
)GLSL";
// Strength applied separately so sunset fade and saved artistic strength remain independent.
inline const std::string kWeatherVertex=std::string(R"GLSL(#version 330 core
uniform mat4 uVP;
uniform vec3 uCamera,uRight,uUp,uDrift;
uniform float uRadius,uTime,uSeed,uDensity,uSize,uVariation,uFall,uFlutter,uTumble,uResponse;
uniform vec4 uTurbulence,uHeight;
uniform int uSlots,uKind;
out vec2 vUV;out vec3 vWorld;out float vAlpha,vSeed;
)GLSL")+kWeatherNoise+R"GLSL(
void main(){
    const vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
    int id=gl_VertexID/6,slot=id%uSlots,cellID=id/uSlots;
    vec3 drift=uDrift*uResponse-vec3(0,0,uFall);
    float cellSize=uRadius/5.;
    vec3 cell=floor((uCamera-drift)/cellSize)+vec3(cellID%13-6,(cellID/13)%13-6,cellID/169-6);
    bool shallow=uHeight.y-uHeight.x<uRadius*2.;
    if(shallow)cell.z=float(cellID/169);
    vec3 key=cell+vec3(float(slot)*17.13,uSeed,43.1);
    vec3 r=vec3(whash(key),whash(key+7.),whash(key+31.));vSeed=whash(key+67.);
    vec3 p=(cell+r)*cellSize+drift;
    if(shallow)p.z=uHeight.x+mod((float(cellID/169)+r.z)/13.*(uHeight.y-uHeight.x)+drift.z,max(.01,uHeight.y-uHeight.x));
    float phase=vSeed*6.2831853;
    vec3 q=p/max(1.,uTurbulence.y)+phase;
    p+=uTurbulence.x*vec3(sin(q.z+uTime*uTurbulence.z+phase),sin(q.x-uTime*uTurbulence.z*.83),sin(q.y+uTime*uTurbulence.z*.61))*.6;
    p.xy+=vec2(sin(uTime*(.8+r.x)+phase),cos(uTime*(.61+r.y)+phase))*uFlutter*12.;
    float distance=length(p-uCamera);
    vAlpha=(1.-smoothstep(uRadius*.68,uRadius,distance))*smoothstep(uHeight.x,uHeight.x+max(1.,uHeight.z),p.z)*(1.-smoothstep(uHeight.y-max(1.,uHeight.z),uHeight.y,p.z));
    if(whash(key+83.)>uDensity)vAlpha=0.;
    vUV=corners[gl_VertexID%6];float angle=phase+uTime*uTumble*(.4+r.z);
    vec2 uv=mat2(cos(angle),-sin(angle),sin(angle),cos(angle))*vUV;
    float size=uSize*mix(1.,.25+1.5*r.x,uVariation);
    if(uKind==2){uv.x*=mix(.28,1.,abs(sin(angle*.67)));uv.y*=1.4;}
    p+=(uRight*uv.x+uUp*uv.y)*size;
    vWorld=p;gl_Position=uVP*vec4(p,1);
    if(vAlpha<=0.)gl_Position=vec4(2,2,2,1);
}
)GLSL";
inline const std::string kWeatherParticleFragment=std::string(R"GLSL(#version 330 core
uniform sampler2D uDepth,uShelter;
uniform vec2 uViewport,uRange;
uniform vec3 uCamera,uColor,uLight,uShelterSlope;
uniform vec3 uTile;
uniform bool uForeground,uSheltered,uFog;
uniform int uKind,uStyle;
uniform float uOpacity,uBrightness,uSoftness,uNear,uTime;
uniform vec3 uAerosol;
uniform vec4 uFogParams;uniform vec3 uFogColor;
in vec2 vUV;in vec3 vWorld;in float vAlpha,vSeed;out vec4 color;
float linearDepth(float d){return 2.*uRange.x*uRange.y/max(.00001,uRange.y+uRange.x-(d*2.-1.)*(uRange.y-uRange.x));}
)GLSL")+kWeatherNoise+R"GLSL(
void main(){
    if(vAlpha<=0.)discard;
    float d=texture(uDepth,gl_FragCoord.xy/uViewport).r;if(uForeground&&d<.0400001)discard;
    float gap=linearDepth(d)-linearDepth(gl_FragCoord.z);if(gap<0.)discard;
    if(uSheltered){vec2 uv=(vWorld.xy+uShelterSlope.xy*vWorld.z-uTile.xy)/(2.*uTile.z)+.5;
        if(all(greaterThan(uv,vec2(0)))&&all(lessThan(uv,vec2(1))))if(vWorld.z<texture(uShelter,uv).r-1.)discard;}
    float r=length(vUV),shape=0.;vec3 tint=uColor;
    if(uKind==0){float edge=max(fwidth(r),.05);float flake=(1.-smoothstep(.15,1.,r));shape=mix(flake,exp(-r*r*5.),vSeed);shape*=1.-smoothstep(1.-edge,1.,r);}
    else if(uKind==1){shape=exp(-dot(vUV,vUV)*4.)*(1.-smoothstep(.75,1.,r));}
    else if(uKind==3){float n=wfbm(vec3(vUV*2.5+vSeed*71.,uTime*.08));shape=pow(max(0.,1.-dot(vUV,vUV)),2.)*smoothstep(.15,.7,n);}
    else if(uStyle==0){float leaf=abs(vUV.x)*1.4+vUV.y*vUV.y;shape=1.-smoothstep(.8,1.,leaf);float vein=1.-smoothstep(.015,.05,abs(vUV.x+.06*sin(vUV.y*8.)));tint*=mix(.6,1.2,vSeed)*(1.-vein*.3);}
    else if(uStyle==1){shape=1.-smoothstep(.35,1.,r);tint=mix(tint,vec3(.7),.5);}
    else {shape=(1.-smoothstep(.55,.65,abs(vUV.x)))*(1.-smoothstep(.7,.8,abs(vUV.y)));tint*=mix(.5,1.2,vSeed);}
    if((uKind==1||uKind==3)&&uAerosol.y>0.){
        vec3 q=vWorld*uAerosol.x*.01+vec3(vSeed*31.,0,uTime*.15);
        float grain=wnoise(q);float footprint=max(length(dFdx(q)),length(dFdy(q)));
        grain=mix(grain,.5,smoothstep(.5,2.,footprint));
        shape*=max(0.,1.+(grain-.5)*2.*uAerosol.y*uAerosol.z);
    }
    float distance=length(vWorld-uCamera);
    float alpha=clamp(vAlpha*shape*uOpacity*smoothstep(0.,max(.01,uSoftness),gap)*smoothstep(uNear,max(uNear+.1,uNear*2.),distance),0.,.95);
    vec3 rgb=tint*uBrightness*uLight;
    if(uFog){float f=clamp((1.-exp2(-max(0.,distance-uFogParams.x)/max(1.,uFogParams.y)))*uFogParams.z,0.,1.);rgb=mix(rgb,uFogColor,f);}
    color=vec4(wfinite(rgb)*alpha,alpha);
}
)GLSL";
inline const std::string kWeatherScreenFragment=std::string(R"GLSL(#version 330 core
in vec2 vScreen;out vec4 color;
uniform sampler2D uDepth,uScene,uLensSurface;
uniform vec2 uViewport,uRange,uInvProjection;
uniform vec3 uCamera,uRight,uUp,uForward,uDrift,uColor;
uniform bool uForeground;
uniform int uMode,uSteps;
uniform float uTime,uSeed;
uniform vec4 uA,uB,uC,uD,uE,uF,uG,uH;
uniform vec2 uSurfaceTexel,uFilter;
float linearDepth(float d){return 2.*uRange.x*uRange.y/max(.00001,uRange.y+uRange.x-(d*2.-1.)*(uRange.y-uRange.x));}
)GLSL")+kWeatherNoise+R"GLSL(
void main(){
vec2 uv=gl_FragCoord.xy/uViewport;

        float depth=texture(uDepth,uv).r;if(uForeground&&depth<.0400001){color=vec4(0);return;}
        vec3 ray=uForward+uRight*((uv.x*2.-1.)*uInvProjection.x)+uUp*((uv.y*2.-1.)*uInvProjection.y);
        float lengthRay=length(ray);ray/=lengthRay;
        float end=min(uB.z,linearDepth(depth)*lengthRay),begin=max(0.,uB.w);
        if(abs(ray.z)>.00001){float a=(uA.x-uCamera.z)/ray.z,b=(uA.y-uCamera.z)/ray.z;begin=max(begin,min(a,b));end=min(end,max(a,b));}
        else if(uCamera.z<uA.x||uCamera.z>uA.y){color=vec4(0);return;}
        if(end<=begin){color=vec4(0);return;}
        float stepLength=(end-begin)/float(uSteps),optical=0.;
        for(int i=0;i<64;++i){if(i>=uSteps)break;vec3 p=uCamera+ray*(begin+(float(i)+.5)*stepLength);
            float h=smoothstep(uA.x,uA.x+uC.x,p.z)*(1.-smoothstep(uA.y-uC.x,uA.y,p.z));
            float n=wfbm(vec3((p.xy-uDrift.xy)/uB.x,p.z/uB.x*.7+uSeed));
            optical+=h*mix(1.,smoothstep(.18,.8,n),uB.y)*stepLength*uA.z*.01;}
        float alpha=clamp((1.-exp(-min(optical,80.)))*uA.w,0.,1.);color=vec4(wfinite(uColor)*alpha,alpha);return;
}
)GLSL";
}
