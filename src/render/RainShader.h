#pragma once
namespace render {
inline constexpr const char* kRainShelterVertex=R"GLSL(#version 330 core
layout(location=0)in vec3 position;layout(location=2)in vec2 uv;
uniform mat4 uModel;uniform vec4 uTile,uWindRange;
out vec3 world;out vec2 texcoord;
void main(){world=(uModel*vec4(position,1)).xyz;texcoord=uv;
gl_Position=vec4((world.xy+uWindRange.xy*world.z-uTile.xy)/uTile.z,1.-2.*(world.z-uWindRange.z)/(uWindRange.w-uWindRange.z),1);}
)GLSL";
inline constexpr const char* kRainShelterFragment=R"GLSL(#version 330 core
uniform sampler2D uAlbedo;uniform float uCutoff;uniform vec4 uWindRange;
in vec3 world;in vec2 texcoord;out vec4 surface;
void main(){if(uCutoff>=0.&&texture(uAlbedo,texcoord).a<uCutoff)discard;
vec3 n=normalize(cross(dFdx(world),dFdy(world)));if(dot(n,vec3(uWindRange.xy,-1))>0.)n=-n;surface=vec4(world.z,n);}
)GLSL";
inline constexpr const char* kRainVertex=R"GLSL(#version 330 core
uniform sampler2D uLanes,uShelter;
uniform bool uHasShelter;
uniform vec3 uShelterTile;
uniform float uEventTime,uCellSize;
uniform vec4 uSplashStyle,uSplashShape;
uniform mat4 uVP;
uniform vec3 uCamera,uSlope;
uniform float uPhase,uSpeed,uRadius,uWidth,uShutter,uDensity,uSplashSize,uSplashAmount;
uniform vec2 uViewport;
uniform bool uSplashes;
uniform vec3 uNoiseTime;
uniform vec2 uGustTime;
uniform float uTurbulence,uSwirlSize,uSwirlRate,uGust,uGustRate,uMistAmount,uMistSize;
uniform float uMistTime;
uniform bool uWallMist;
out vec2 vUV;out vec3 vWorld;out float vAlpha;out float vAge;out float vSeed;
flat out int vMode;
const vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
float hash(float n){return fract(sin(n*127.1+311.7)*43758.5453);}
vec3 curl(vec3 p,vec3 t){
    // Analytic curl of three independent Fourier potentials. Two incommensurate
    // bands provide smooth vortices without particle simulation/history.
    vec3 k1=vec3(.31,1.13,.73),k2=vec3(.91,.27,1.21),k3=vec3(1.07,.67,.19);
    vec3 c=cos(vec3(dot(p,k1),dot(p,k2),dot(p,k3))+t);
    return vec3(k3.y*c.z-k2.z*c.y,k1.z*c.x-k3.x*c.z,k2.x*c.y-k1.y*c.x);
}
vec3 trajectory(vec4 b,float z,float secondsBack){
    vec3 p=vec3(b.xy-uSlope.xy*z,z);
    vec3 t=uNoiseTime-vec3(.61,-.83,.47)*uSwirlRate*secondsBack;
    vec3 q=p/max(30.,uSwirlSize);
    vec3 flow=curl(q,t)+curl(q*2.17+vec3(17,29,7),t.zxy)*.32;
    float gust=sin(uGustTime.x-uGustRate*secondsBack+dot(p.xy,vec2(.0017,.0011)))
       +.35*sin(uGustTime.y-uGustRate*.713*secondsBack+dot(p.xy,vec2(-.0021,.0029)));
    vec3 wind=vec3(uSlope.xy,0);wind=length(wind)>.0001?normalize(wind):vec3(1,0,0);
    vec3 offset=flow*uTurbulence+wind*(gust*uGust);
    // Anchor the analytic stream at its verified collision point. Do not move
    // its impact through a roof/wall as the turbulent displacement changes.
    float contact=smoothstep(0.,max(160.,uTurbulence*5.+uGust*3.),z-b.z);
    offset.z*=.25;
    return p+offset*contact;
}
void main(){
    int particle=gl_VertexID/6,lane=particle/16,drop=particle%16;
    vec4 b=texelFetch(uLanes,ivec2(lane%256,(lane/256)*2),0);
    vec4 n=texelFetch(uLanes,ivec2(lane%256,(lane/256)*2+1),0);
    // Disabled impact types need no shelter lookup or trajectory evaluation.
    if((drop>=12&&drop<14&&!uSplashes)||(drop>=14&&!uWallMist)){
        vAlpha=0.;vUV=vec2(0);vWorld=vec3(0);vAge=0.;vSeed=0.;vMode=0;gl_Position=vec4(2,2,2,1);return;
    }
    // One deterministic position per impact event, not one repeating splash per
    // lane. Ring and droplets share the event; mist has its own slower clock.
    float eventAge=0.;
    if(drop>=12){
        float rate=drop<14?mix(2.1,4.7,n.w)*max(.001,abs(uSplashStyle.x)):.65;
        float event=uEventTime*rate+b.w*19.;eventAge=fract(event)/rate;
        float key=b.w*173.+floor(event)*.731;
        b.xy+=(vec2(hash(key),hash(key+93.7))-.5)*uCellSize*uSplashStyle.z;
        n.w=hash(key+17.);b.w=hash(key+57.);
    }
    if(uHasShelter){vec2 uv=(b.xy-uShelterTile.xy)/(2.*uShelterTile.z)+.5;
        if(all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)))){
            vec4 surface=texture(uShelter,uv);b.z=surface.x;n.xyz=surface.yzw;
            vec2 sampleUv=(floor(uv*vec2(textureSize(uShelter,0)))+.5)/vec2(textureSize(uShelter,0));
            vec2 sampleLane=(sampleUv-.5)*(2.*uShelterTile.z)+uShelterTile.xy;
            float denom=n.z-dot(n.xy,uSlope.xy);if(abs(denom)>.0001&&b.z> -1e7)b.z-=dot(n.xy,b.xy-sampleLane)/denom;
        }}
    if(dot(n.xyz,vec3(uSlope.xy,-1))>0.)n.xyz=-n.xyz;
    vUV=corners[gl_VertexID%6];vMode=drop<12?0:drop<14?drop-11:3;vSeed=b.w;
    float seed=hash(b.w*900.+float(drop));vAlpha=seed<uDensity?1.:0.;vAge=0.;
    vec3 velocity=vec3(uSlope.xy,-1)*uSpeed;
    vec3 p;
    if(vMode==0){
        float lower=uCamera.z-1200.;
        float z=lower+mod(b.w*2400.+float(drop)*200.-uPhase-lower,2400.);
        vAlpha*=1.-smoothstep(850.,1150.,abs(z-uCamera.z));
        float exposure=uShutter*mix(.65,1.25,seed);
        vec3 head=trajectory(b,z,0.),tail=trajectory(b,z+uSpeed*exposure,exposure);
        p=(head+tail)*.5;
        vec3 axis=normalize(head-tail),toCamera=normalize(uCamera-p);
        vec3 side=cross(axis,toCamera);float sl=length(side);
        side=sl>.0001?side/sl:vec3(1,0,0);
        float lengthCm=max(.01,length(head-tail));
        // A subpixel streak retains coverage instead of flickering out.
        vec4 center=uVP*vec4(p,1);float pixelCm=2.*abs(center.w)/(uViewport.y*length(vec3(uVP[0][1],uVP[1][1],uVP[2][1])));
        float authored=uWidth*mix(.7,1.3,seed),width=max(authored,pixelCm*.9);
        vAlpha*=authored/width;
        p+=axis*(vUV.y*lengthCm*.5)+side*(vUV.x*width);
        vAlpha*=smoothstep(0.,3.,p.z-b.z);
    }else if(vMode<3){
        vAge=eventAge;
        if(!uSplashes||b.z< -1e7||n.w>uSplashAmount||n.z<.2)vAlpha=0.;
        vec3 normal=normalize(n.xyz),tangent=normalize(cross(abs(normal.z)>.9?vec3(0,1,0):vec3(0,0,1),normal));
        vec3 bitangent=cross(normal,tangent);
        p=vec3(b.xy-uSlope.xy*b.z,b.z)+normal*.7;
        float age=clamp(vAge/max(.001,abs(uSplashStyle.y)),0.,1.);vAge=age;
        vAlpha*=pow(1.-age,max(.001,uSplashStyle.w));
        if(vMode==1){p+=(tangent*vUV.x+bitangent*vUV.y)*uSplashSize*(.35+age*uSplashShape.x);}
        else{
            vec3 side=cross(normal,normalize(uCamera-p));
            side=length(side)<.01?tangent:normalize(side);
            p+=side*vUV.x*uSplashSize*uSplashShape.z+normal*(vUV.y+1.)*uSplashSize*uSplashShape.y;
        }
    }else{
        vec3 normal=normalize(n.xyz);
        if(!uWallMist||b.z< -1e7||abs(normal.z)>.6)vAlpha=0.;
        float age=eventAge*.65;vAge=age;
        vAlpha*=uMistAmount*sin(age*3.14159)*.25;
        vec3 impact=vec3(b.xy-uSlope.xy*b.z,b.z);
        vec3 tangentWind=velocity-normal*dot(velocity,normal);
        p=impact+normal*(3.+age*uMistSize*.35)+tangentWind*(age*.04);
        vec3 toCamera=normalize(uCamera-p),side=cross(vec3(0,0,1),toCamera);
        side=length(side)<.01?vec3(1,0,0):normalize(side);
        vec3 up=normalize(cross(toCamera,side));
        p+=(side*vUV.x+up*vUV.y)*uMistSize*(.4+age);
    }
    float distance=length(p-uCamera);
    vAlpha*=1.-smoothstep(uRadius*.7,uRadius,distance);
    vWorld=p;gl_Position=uVP*vec4(p,1);
}
)GLSL";
inline constexpr const char* kRainFragment=R"GLSL(#version 330 core
uniform sampler2D uDepth;
uniform sampler2D uRainBackground;
uniform vec4 uSplashDetail,uRefraction;
uniform vec3 uAerosol;
uniform bool uRefractive;
uniform vec2 uViewport,uRange;
uniform vec3 uCamera,uTint,uLight,uSun,uFogColor;
uniform float uOpacity,uNearFade,uSoftness,uBrightness,uFogStart,uFogHalf,uFogOpacity;
uniform float uMistSize;
uniform bool uForeground,uFog,uHeightFog;
uniform float uFogHeight,uFogFalloff;
in vec2 vUV;in vec3 vWorld;in float vAlpha;in float vAge;in float vSeed;flat in int vMode;
out vec4 color;
float linear(float d){return uRange.x*uRange.y/max(.00001,uRange.y-d*(uRange.y-uRange.x));}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);vec4 h=fract(sin(vec4(dot(i,vec2(127.1,311.7)),dot(i+vec2(1,0),vec2(127.1,311.7)),dot(i+vec2(0,1),vec2(127.1,311.7)),dot(i+vec2(1,1),vec2(127.1,311.7))))*43758.5453);return mix(mix(h.x,h.y,f.x),mix(h.z,h.w,f.x),f.y);}
void main(){
    if(vAlpha<.00001)discard;
    float d=texture(uDepth,gl_FragCoord.xy/uViewport).r;
    if(uForeground&&d<.0400001)discard;
    float gap=linear(d)-linear(gl_FragCoord.z);if(gap<0.)discard;
    float soft=vMode==3?max(uSoftness,uMistSize*.8):vMode==0?uSoftness:.2;
    float alpha=vAlpha*uOpacity*smoothstep(0.,soft,gap);
    float distance=length(vWorld-uCamera);alpha*=smoothstep(uNearFade,uNearFade*2.,distance);
    if(vMode==0){alpha*=exp(-vUV.x*vUV.x*4.5)*pow(max(0.,1.-vUV.y*vUV.y),1.2);}
    else if(vMode==1){
        float r=length(vUV),w=max(fwidth(r),abs(uSplashDetail.y));
        alpha*=(1.-smoothstep(w,w*2.,abs(r-.68)))*(1.-smoothstep(.8,1.,r));alpha*=uSplashDetail.x;
    }else if(vMode==2){
        float a=vAge;float y=(vUV.y+1.)*.5;
        float height=4.*a*(1.-a)*.75;
        float drops=0.;
        int count=int(clamp(uSplashDetail.w,1.,32.));
        for(int i=0;i<32;++i){if(i>=count)break;float seed=noise(vec2(float(i)*17.,vSeed*37.));float x=((float(i)+seed)/float(count)*2.-1.)*(.25+a);vec2 delta=vec2((vUV.x-x)/.075,(y-height*(.65+.35*seed))/.07)/max(.01,abs(uSplashDetail.z));drops+=exp(-dot(delta,delta)*2.);}
        alpha*=drops;
    }else{
        vec2 q=vUV*3.+vec2(vSeed*47.,vAge*2.);
        float detail=noise(q)*.6+noise(q*2.13+13.)*.3+noise(q*4.37)*.1;
        alpha*=pow(max(0.,1.-dot(vUV,vUV)),2.)*smoothstep(.15,.7,detail);
        vec2 grainUV=vUV*uAerosol.x+vec2(vSeed*71.,vAge*13.);
        float grain=clamp((noise(grainUV)-.5)*uAerosol.z+.5,0.,1.);
        float resolved=1.-smoothstep(.4,1.5,max(length(dFdx(grainUV)),length(dFdy(grainUV))));
        alpha*=mix(1.,grain,uAerosol.y*resolved);
    }
    if(alpha<.0001)discard;
    float glint=pow(max(0.,dot(normalize(uCamera-vWorld),-uSun)),8.);
    vec3 radiance=uTint*uBrightness*uLight*(.65+glint*.8);
    if(uFog){float density=uHeightFog?exp(clamp((uFogHeight-vWorld.z)/max(1.,uFogFalloff),-12.,12.)):1.;float f=clamp((1.-exp2(-max(0.,distance-uFogStart)*density/max(1.,uFogHalf)))*uFogOpacity,0.,1.);radiance=mix(radiance,uFogColor,f);}
    if(uRefractive&&vMode==0){
        vec2 uv=gl_FragCoord.xy/uViewport;
        vec2 offset=vec2(vUV.x,vUV.y*.2)*uRefraction.w*(1.-1./max(1.0001,uRefraction.y))/uViewport;
        vec2 target=clamp(uv+offset,vec2(.5)/uViewport,1.-vec2(.5)/uViewport);
        if(linear(texture(uDepth,target).r)<linear(gl_FragCoord.z))target=uv;
        vec2 blur=vec2(abs(uRefraction.z)*8.)/uViewport;
        vec3 refracted=texture(uRainBackground,target).rgb*.4;
        refracted+=(texture(uRainBackground,target+vec2(blur.x,0)).rgb+texture(uRainBackground,target-vec2(blur.x,0)).rgb+texture(uRainBackground,target+vec2(0,blur.y)).rgb+texture(uRainBackground,target-vec2(0,blur.y)).rgb)*.15;
        radiance=mix(radiance,refracted,clamp(uRefraction.x,0.,1.));
    }
    alpha=clamp(alpha,0.,.85);color=vec4(radiance*alpha,alpha);
}
)GLSL";
}
