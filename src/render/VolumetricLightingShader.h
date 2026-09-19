#pragma once
namespace render {
inline constexpr const char* kVolumetricLightingFragment=R"GLSL(
#version 330 core
in vec2 vScreen;out vec4 color;
uniform sampler2D uDepth,uVolume,uShadow,uFarShadow;
uniform int uMode,uSteps;uniform bool uForeground,uShadows,uFarShadows;
uniform vec2 uRange,uInvProjection,uLowSize;
uniform vec3 uCamera,uRight,uUp,uForward,uSun,uRadiance;
uniform mat4 uLight,uFarLight;
uniform float uDensity,uIntensity,uHeight,uHeightFalloff,uAnisotropy,uMaxDistance,uBias;
uniform vec4 uCascade;uniform float uFarBlend;
bool foreground(float d){return uForeground&&d<.0400001;}
float linearDepth(float d){return 2.0*uRange.x*uRange.y/(uRange.y+uRange.x-(d*2.0-1.0)*(uRange.y-uRange.x));}
float shadowMap(sampler2D map,mat4 matrix,vec3 point){vec4 q=matrix*vec4(point,1);vec3 p=q.xyz/q.w*.5+.5;
 if(any(lessThanEqual(p,vec3(0)))||any(greaterThanEqual(p,vec3(1))))return 1.0;
 // Ray integration already averages many spatial shadow tests. A single
 // comparison per step avoids multiplying this pass by surface-light PCF cost.
 return p.z-uBias<=texture(map,p.xy).r?1.0:0.0;
}
float visibility(vec3 p,float d){if(!uShadows)return 1.0;float n=shadowMap(uShadow,uLight,p);
 if(!uFarShadows)return mix(n,1.0,smoothstep(uCascade.x,uCascade.y,d));
 float blend=max(.001,uFarBlend),f=smoothstep(uCascade.z-blend,uCascade.z+blend,d);
 float farWeight=f*(1.0-smoothstep(uCascade.w-blend,uCascade.w,d));
 return mix(1.0,n,1.0-f)*(1.0-farWeight)+shadowMap(uFarShadow,uFarLight,p)*farWeight;
}
void main(){vec2 uv=vScreen*.5+.5;float raw=texture(uDepth,uv).r;
 if(foreground(raw)){color=vec4(0);return;}float z=linearDepth(raw);
 if(uMode==1){
   // Four-tap depth-aware reconstruction never borrows from viewmodel pixels.
   vec2 p=uv*uLowSize-.5,b=floor(p),f=fract(p);vec3 sum=vec3(0);float weights=0;
   for(int y=0;y<2;++y)for(int x=0;x<2;++x){vec2 tap=clamp((b+vec2(x,y)+.5)/uLowSize,vec2(0),vec2(1));vec4 v=texture(uVolume,tap);
     if(v.a<=0)continue;float w=(x==0?1.0-f.x:f.x)*(y==0?1.0-f.y:f.y)*exp(-abs(v.a-z)/max(5.0,z*.015));sum+=v.rgb*w;weights+=w;}
   color=vec4(weights>.00001?sum/weights:vec3(0),0);return;
 }
 vec2 xy=(uv*2.0-1.0)*uInvProjection;vec3 ray=normalize(uForward+uRight*xy.x+uUp*xy.y);
 float distance=min(uMaxDistance,z/max(.0001,dot(ray,uForward))),ds=distance/float(uSteps),T=1.0;vec3 scatter=vec3(0);
 float g=uAnisotropy,c=dot(ray,-uSun),phase=(1.0-g*g)/pow(max(.001,1.0+g*g-2.0*g*c),1.5);
 // Fixed spatial jitter: fractional playback and backwards seeks have no history.
 float jitter=fract(sin(dot(floor(gl_FragCoord.xy),vec2(12.9898,78.233)))*43758.5453);
 for(int i=0;i<64;++i){if(i>=uSteps)break;float d=(float(i)+jitter)*ds;vec3 p=uCamera+ray*d;
   float density=uDensity*exp2(-max(0.0,p.z-uHeight)/uHeightFalloff),attenuation=exp(-density*ds);
   scatter+=T*(1.0-attenuation)*visibility(p,d)*phase*uRadiance*uIntensity;T*=attenuation;
 }
 color=vec4(scatter,z);
})GLSL";
}
