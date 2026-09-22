#pragma once
namespace render {
inline constexpr const char* kWaterVertex=R"GLSL(
#version 330 core
layout(location=0) in vec2 aGrid;
uniform mat4 uVP;
uniform vec3 uCenter;
uniform vec3 uGridForward,uGridRight,uGridUp;
uniform vec2 uGridTan;
uniform float uGridHeight,uGridTop;
uniform bool uProjectedGrid;
uniform float uRadius,uChop,uResolution;
uniform sampler2D uCrestProfile;
uniform vec4 uWaves[16];
uniform float uPhases[16];
out vec3 vWorld,vNormal;
out vec2 vSurface;
out float vCrest,vCell;
vec2 projectGrid(vec2 grid){
    if(!uProjectedGrid)return uCenter.xy+uRadius*(.035*grid+.965*grid*grid*grid);
    vec2 screen=vec2(grid.x*1.65,mix(-1.8,uGridTop,grid.y*.5+.5));
    vec3 ray=uGridForward+uGridRight*(screen.x*uGridTan.x)+uGridUp*(screen.y*uGridTan.y);
    float t=min(uGridHeight/max(.0001,-ray.z),uRadius/max(.001,length(ray.xy)));
    return uCenter.xy+ray.xy*t;
}
void main(){
    // Project the same fixed-size grid into the visible footprint. The old
    // radial grid spent most vertices behind/below the camera.
    vec2 xy=projectGrid(aGrid);
    vec3 p=vec3(xy,uCenter.z),dx=vec3(1,0,0),dy=vec3(0,1,0);
    float crest=0;
    float stepSize=2/uResolution;
    float cell=max(length(projectGrid(aGrid+vec2(stepSize,0))-xy),
                   length(projectGrid(aGrid+vec2(0,stepSize))-xy));
    for(int i=0;i<16;++i){
        vec4 w=uWaves[i];float ph=dot(w.xy,xy)*w.w+uPhases[i];
        // Geometry must not represent waves shorter than its local sampling
        // grid. Their normals are evaluated per pixel instead.
        w.z*=1-smoothstep(1.0,3.0,cell*w.w);
        float s=sin(ph),c=cos(ph),ak=w.z*w.w;
        // A sharp profile contains harmonics above its fundamental. Integrate
        // it over two mesh cells before forming the silhouette.
        float lod=max(0,log2(max(.001,2*cell*w.w)*2048/6.2831853));
        vec2 profile=textureLod(uCrestProfile,vec2(ph/6.2831853,.5),lod).rg;
        float slope=profile.y;
        p+=vec3(uChop*w.z*w.xy*c,w.z*profile.x);
        dx+=vec3(-uChop*ak*w.xy*w.x*s,ak*w.x*slope);
        dy+=vec3(-uChop*ak*w.xy*w.y*s,ak*w.y*slope);
        crest+=ak*s;
    }
    vWorld=p;vSurface=xy;vNormal=normalize(cross(dx,dy));vCrest=crest;vCell=cell;
    gl_Position=uVP*vec4(p,1);
}
)GLSL";
inline constexpr const char* kWaterFragment=R"GLSL(
#version 330 core
in vec3 vWorld,vNormal;
in vec2 vSurface;
in float vCrest,vCell;
out vec4 fragColor;
uniform vec3 uCamera,uColor,uSun,uSunColor,uAmbient,uFogColor;
uniform float uRoughness,uReflection,uDetail,uFoam,uTime,uRain,uRainScale,uRainSpeed;
uniform vec4 uWaves[16];
uniform float uPhases[16],uChop,uWaterHeight,uWaveHeight;
uniform sampler2D uSceneDepth,uSceneColor;
uniform sampler2D uCrestProfile,uFoamTexture;
uniform float uFoamPersistence,uFoamDetail;
uniform bool uDepthEnabled;
uniform vec2 uViewport,uRange;
uniform float uPixelScale;
uniform vec3 uForward,uShallow;
uniform float uAbsorption,uTransmission,uCrestLight,uFoamCoverage,uFoamScale,uShoreFoam,uShoreWidth;
uniform float uFogStart,uFogHalf,uFogOpacity,uFogHeight,uFogFalloff;
uniform bool uFog,uHeightFog;
uniform int uDebug,uSkyMode;
uniform sampler2D uFaces[6];
uniform int uSources[6],uTurns[6];
uniform float uSkyRotation,uSkyIntensity,uSkyExposure;
uniform float uSkyContrast,uSkyHighlightThreshold,uSkyHighlightBoost,uSkyToneMap;
uniform bool uSkyFlip;
uniform mat4 uLight,uFarLight;
uniform sampler2D uShadow,uFarShadow;
uniform bool uShadows,uFarShadows;
uniform float uShadowBias,uShadowStrength,uShadowDistance,uFarShadowDistance;
float shadowTap(sampler2D tex,mat4 light,float radius){
    vec4 p=light*vec4(vWorld,1);vec3 q=p.xyz/p.w*.5+.5;
    if(any(lessThan(q,vec3(0)))||any(greaterThan(q,vec3(1))))return 1;
    vec2 px=1.0/vec2(textureSize(tex,0));float lit=0;
    float biasScale=(4.*radius-1.)*.5*length(vec3(light[0].z,light[1].z,light[2].z));
    for(int y=0;y<2;++y)for(int x=0;x<2;++x)
        lit+=q.z-uShadowBias*biasScale<=texture(tex,q.xy+(vec2(x,y)-.5)*px).r?1:0;
    return mix(1,lit*.25,uShadowStrength);
}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){
    vec2 i=floor(p),f=fract(p);f=f*f*(3-2*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1),f.x),f.y);
}
float linearDepth(float d){return 2*uRange.x*uRange.y/(uRange.y+uRange.x-(2*d-1)*(uRange.y-uRange.x));}
vec3 rawFace(int f,vec2 uv){
    if(f==0)return texture(uFaces[0],uv).rgb;if(f==1)return texture(uFaces[1],uv).rgb;
    if(f==2)return texture(uFaces[2],uv).rgb;if(f==3)return texture(uFaces[3],uv).rgb;
    if(f==4)return texture(uFaces[4],uv).rgb;return texture(uFaces[5],uv).rgb;
}
vec3 environment(vec3 r){
    if(uSkyMode==0)return mix(vec3(.18,.23,.3),vec3(.45,.65,.85),smoothstep(-.1,.8,r.z))*max(vec3(.08),uAmbient);
    float c=cos(uSkyRotation),s=sin(uSkyRotation);
    vec3 d=vec3(r.x*c-r.y*s,-(r.x*s+r.y*c),uSkyFlip?-r.z:r.z);
    vec3 rgb;
    if(uSkyMode==1)rgb=rawFace(0,vec2(atan(d.y,d.x)/6.2831853+.5,asin(clamp(d.z,-1,1))/3.14159265+.5));
    else {
        vec3 a=abs(d);vec2 uv;int f;
        if(a.x>=a.y&&a.x>=a.z){if(d.x>0){f=0;uv=vec2(-d.z,d.y)/a.x;}else{f=1;uv=vec2(d.z,d.y)/a.x;}}
        else if(a.y>=a.z){if(d.y>0){f=2;uv=vec2(d.x,-d.z)/a.y;}else{f=3;uv=vec2(d.x,d.z)/a.y;}}
        else{if(d.z>0){f=4;uv=vec2(d.x,d.y)/a.z;}else{f=5;uv=vec2(-d.x,d.y)/a.z;}}
        uv=uv*.5+.5;int q=uTurns[f]%4;if(q==1)uv=vec2(1-uv.y,uv.x);else if(q==2)uv=1-uv;else if(q==3)uv=vec2(uv.y,1-uv.x);
        rgb=rawFace(uSources[f],uv);
    }
    rgb*=uSkyIntensity*exp2(uSkyExposure);rgb=max(vec3(0),.5+(rgb-.5)*uSkyContrast);
    float l=max(rgb.r,max(rgb.g,rgb.b));
    rgb*=mix(1.0,uSkyHighlightBoost,smoothstep(uSkyHighlightThreshold,max(uSkyHighlightThreshold+.001,1.0),l));
    return mix(rgb,rgb/(rgb+vec3(1)),clamp(uSkyToneMap,0.0,1.0));
}
void main(){
    vec3 view=normalize(uCamera-vWorld),n=normalize(vNormal);
    float distanceToCamera=length(uCamera-vWorld);
    vec2 p=vSurface*.01;
    vec3 dx=vec3(1,0,0),dy=vec3(0,1,0);
    vec3 previousCompression[3];
    for(int age=0;age<3;++age)previousCompression[age]=vec3(0);
    // Derivatives of a coarse displaced triangle jump across its edges. Using
    // that value as the profile LOD exposes the hidden tessellation in specular
    // lighting. A continuous perspective footprint avoids those polygon seams.
    float footprint=uPixelScale*distanceToCamera/max(.12,abs(view.z));
    for(int i=0;i<16;++i){
        vec4 w=uWaves[i];
        float ph=dot(w.xy,vSurface)*w.w+uPhases[i],s=sin(ph),c=cos(ph);
        float geometricBand=1-smoothstep(1.0,3.0,vCell*w.w);
        float ak=w.z*w.w*(1-smoothstep(.6,2.5,footprint*w.w));
        // The main lighting slope must agree with the filtered geometric
        // surface. Unresolved harmonics are small normal detail, not full-height
        // fictitious waves that reveal each triangle's visibility boundaries.
        float profileLod=max(0,log2(max(.001,footprint*w.w)*2048/6.2831853));
        float meshLod=max(profileLod,max(0,log2(max(.001,2*vCell*w.w)*2048/6.2831853)));
        float baseSlope=textureLod(uCrestProfile,vec2(ph/6.2831853,.5),meshLod).g;
        float fineSlope=textureLod(uCrestProfile,vec2(ph/6.2831853,.5),profileLod).g;
        float slope=geometricBand*baseSlope+.12*(fineSlope-geometricBand*baseSlope);
        dx+=vec3(-uChop*ak*geometricBand*w.xy*w.x*s,ak*w.x*slope);
        dy+=vec3(-uChop*ak*geometricBand*w.xy*w.y*s,ak*w.y*slope);
        for(int age=0;age<3;++age){
            float past=uChop*ak*sin(ph+sqrt(981*w.w)*uFoamPersistence*float(age+1)/3);
            previousCompression[age]+=past*vec3(w.x*w.x,w.x*w.y,w.y*w.y);
        }
    }
    n=normalize(cross(dx,dy));
    float compression=clamp(1-(dx.x*dy.y-dx.y*dy.x),0,1);
    float foamSignal=compression;
    for(int age=0;age<3;++age){
        vec3 pc=previousCompression[age];
        float old=clamp(1-((1-pc.x)*(1-pc.z)-pc.y*pc.y),0,1);
        foamSignal=max(foamSignal,old*exp(-float(age+1)*.28));
    }
    float detailFade=(1-smoothstep(1500,15000,distanceToCamera))*(1-smoothstep(.15,1.5,length(fwidth(p))*16));
    vec2 perturb=vec2(cos(p.x*8+p.y*3+uTime*1.7)+.5*cos(p.x*17-p.y*9-uTime*2.3),
        sin(p.y*11-p.x*4-uTime*1.3)+.5*sin(p.y*21+p.x*7+uTime*2.1))*.045*uDetail*detailFade;
    if(uRain>0&&detailFade>0){
        vec2 rp=p/uRainScale,cell=floor(rp);
        float rt=uTime*uRainSpeed;
        for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
            vec2 id=cell+vec2(x,y);float seed=hash(id);
            float cycle=floor(rt+seed),age=fract(rt+seed);
            vec2 center=id+vec2(hash(id+cycle+9),hash(id-cycle+31));
            vec2 delta=rp-center;float d=length(delta);
            float ring=d-age*.85;
            float envelope=exp(-ring*ring*160)*sin(age*3.14159265)*(1-smoothstep(.65,1,d));
            float resolved=1-smoothstep(.5,2.5,fwidth(d)*55);
            perturb+=delta/max(d,.001)*cos(ring*55)*envelope*.18*uRain*detailFade*resolved;
        }
    }
    n=normalize(n+vec3(perturb,0));if(!gl_FrontFacing)n=-n;
    if(uDebug==7){fragColor=vec4(1);return;}
    if(uDebug==4||uDebug==6){fragColor=vec4(n*.5+.5,1);return;}
    if(uDebug!=0){fragColor=vec4(uColor,1);return;}
    float nv=clamp(dot(n,view),.001,1.0),f=.02+.98*pow(1-nv,5);
    vec3 refl=reflect(-view,n);
    vec3 tangent=normalize(cross(abs(refl.z)<.95?vec3(0,0,1):vec3(0,1,0),refl)),bitangent=cross(refl,tangent);
    float spread=uRoughness*uRoughness*.6;
    vec3 env=environment(refl)*.4;
    env+=(environment(normalize(refl+tangent*spread))+environment(normalize(refl-tangent*spread))
        +environment(normalize(refl+bitangent*spread))+environment(normalize(refl-bitangent*spread)))*.15;
    vec3 light=normalize(-uSun),halfway=normalize(light+view);
    float nl=clamp(dot(n,light),0,1),nh=clamp(dot(n,halfway),0,1),vh=clamp(dot(view,halfway),0,1);
    float a=max(.002,uRoughness*uRoughness),a2=a*a,den=max(1e-8,(1-nh*nh)+nh*nh*a2);
    float D=a2/(3.14159265*den*den),k=(uRoughness+1)*(uRoughness+1)/8;
    float G=(nv/(nv*(1-k)+k))*(nl/(nl*(1-k)+k));
    float shadow=1;
    if(uShadows){if(distanceToCamera<uShadowDistance)shadow=shadowTap(uShadow,uLight,max(400.,uShadowDistance));
        else if(uFarShadows&&distanceToCamera<uFarShadowDistance)shadow=shadowTap(uFarShadow,uFarLight,max(400.,max(uShadowDistance,uFarShadowDistance)));}
    vec3 sun=uSunColor*shadow;
    // Thickness along the actual camera ray; opaque pre-water depth only.
    // This never samples the water's own attached depth texture.
    vec2 screen=gl_FragCoord.xy/uViewport;
    float thickness=uAbsorption*800,verticalDepth=thickness;
    vec3 floorColor=vec3(0);
    if(uDepthEnabled){
        float d=texture(uSceneDepth,screen).r;
        if(d<.9999999){
            float forwardDistance=linearDepth(d);
            float surfaceDistance=dot(vWorld-uCamera,uForward);
            thickness=max(0,(forwardDistance-surfaceDistance)/max(.02,dot(-view,uForward)));
            verticalDepth=thickness*abs(view.z);
        }
        floorColor=texture(uSceneColor,screen).rgb;
    }
    float shallow=exp(-verticalDepth/max(1,uAbsorption*100));
    vec3 body=mix(uColor,uShallow,shallow);
    vec3 transmittance=exp(-thickness/max(1,uAbsorption*100)*vec3(1.8,.65,.45));
    vec3 lighting=uAmbient+sun*nl*.25;
    vec3 color=body*lighting;
    if(uDepthEnabled)color=mix(color,floorColor,transmittance*uTransmission);
    float crestHeight=clamp((vWorld.z-uWaterHeight)/max(1,uWaveHeight*60),0,1);
    float backlight=pow(max(dot(-light,view),0),3);
    color+=uShallow*sun*uCrestLight*(.18+backlight)*crestHeight*(.3+.7*compression);
    color=color*(1-f)+env*f*uReflection;
    color+=sun*D*G*(.02+.98*pow(1-vh,5))/(4*nv+.001);
    // Coverage is separate from bubble structure, as in production ocean
    // materials. Coordinates follow the displaced surface, not a screen overlay.
    vec2 foamUV=p/max(.1,uFoamScale);
    vec2 warp=vec2(noise(foamUV*.31),noise(foamUV*.31+17))-.5;
    foamUV+=warp*.34;
    vec3 bubble=texture(uFoamTexture,foamUV).rgb;
    vec3 bubble2=texture(uFoamTexture,mat2(.8,-.6,.6,.8)*foamUV*1.713+vec2(13.7,6.1)).rgb;
    float grain=noise(foamUV*.77)*.6+noise(foamUV*2.13)*.4;
    float crestFoam=smoothstep(mix(.86,.12,uFoamCoverage),mix(.99,.60,uFoamCoverage),foamSignal);
    // Thin filaments at low coverage; microbubbles merge into dense fresh foam.
    float structure=mix(bubble.r,bubble2.r,.25)*.75+mix(bubble.g,bubble2.g,.5)*.25;
    float aa=max(.035,fwidth(structure));
    float shore=0;
    if(uDepthEnabled){
        shore=(1-smoothstep(0,uShoreWidth*100,thickness))*uShoreFoam;
        shore*=.65+.35*sin(thickness/max(1,uShoreWidth*100)*14-uTime*1.8+grain*3);
    }
    float density=clamp(crestFoam*uFoam+shore,0,1);
    float threshold=density*1.15-.12+(grain-.5)*.24;
    float bubbles=smoothstep(structure-aa,structure+aa,threshold);
    float foam=mix(density,bubbles*sqrt(density),uFoamDetail);
    // Rough diffuse foam replaces specular water only where bubbles exist.
    // Small luminance relief avoids the old flat white patches.
    float relief=.82+.18*bubble2.g;
    vec3 foamColor=vec3(.70,.76,.74)*(uAmbient+sun*(.12+.27*nl))*relief;
    color=mix(color,foamColor,foam);
    if(uFog){
        float density=uHeightFog?exp(clamp((uFogHeight-vWorld.z)/max(1,uFogFalloff),-12,12)):1;
        float fog=(1-exp2(-max(0,distanceToCamera-uFogStart)*density/max(1,uFogHalf)))*uFogOpacity;
        color=mix(color,uFogColor,clamp(fog,0,1));
    }
    // Keep HDR highlights representable in the half-float scene buffer.
    // Infinity entering bloom/tonemapping can turn a bright reflection black.
    fragColor=vec4(clamp(color,vec3(0),vec3(60000)),1);
}
)GLSL";
}
