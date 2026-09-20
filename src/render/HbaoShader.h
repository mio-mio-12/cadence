#pragma once
namespace render {
// Original GLSL implementation of depth-only horizon AO. Background is fully
// visible; view-space radius and bilateral filtering prevent depth-edge halos.
inline constexpr const char* kHbaoFragment=R"GLSL(
#version 330 core
in vec2 vScreen;out vec4 color;
uniform sampler2D uDepth,uAo;
uniform vec2 uTexel,uInvProjection,uBlurAxis;
uniform float uNear,uFar,uRadius,uBias,uFalloff,uSharpness,uMaxPixels;
uniform int uDirections,uSteps,uBlurRadius,uMode;
uniform bool uForeground,uWeaponBackgroundHalo;
uniform vec2 uViewmodelDepthRange;
bool foreground(float raw){return uForeground&&raw<.0400001;}
float depthAt(vec2 uv){float z=texture(uDepth,uv).r;vec2 range=vec2(uNear,uFar);if(foreground(z)){z=clamp(z/.04,0.0,1.0);range=uViewmodelDepthRange;}return 2.0*range.x*range.y/(range.y+range.x-(z*2.0-1.0)*(range.y-range.x));}
vec3 positionAt(vec2 uv){float z=depthAt(uv);return vec3((uv*2.0-1.0)*uInvProjection*z,-z);}
bool outside(vec2 uv){return any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1)));}
void main(){
 vec2 uv=vScreen*.5+.5;
 if(uMode==1){
  vec2 center=texture(uAo,uv).rg;float sum=center.r,weights=1.0;
  for(int i=-8;i<=8;++i){if(i==0||abs(i)>uBlurRadius)continue;vec2 p=uv+uBlurAxis*uTexel*float(i);if(outside(p))continue;
   vec2 value=texture(uAo,p).rg;float dz=abs(value.g-center.g)/max(1.0,uRadius);
   float w=exp(-2.0*float(i*i)/float(max(1,uBlurRadius*uBlurRadius))-dz*uSharpness);sum+=value.r*w;weights+=w;
  }color=vec4(sum/weights,center.g,0,1);return;
 }
 float raw=texture(uDepth,uv).r;bool fg=foreground(raw);if(raw>=.9999999){color=vec4(1,uFar,0,1);return;}
 vec3 p=positionAt(uv),l=positionAt(clamp(uv-vec2(uTexel.x,0),vec2(0),vec2(1))),r=positionAt(clamp(uv+vec2(uTexel.x,0),vec2(0),vec2(1)));
 vec3 d=positionAt(clamp(uv-vec2(0,uTexel.y),vec2(0),vec2(1))),u=positionAt(clamp(uv+vec2(0,uTexel.y),vec2(0),vec2(1)));
 if(foreground(texture(uDepth,uv-vec2(uTexel.x,0)).r)!=fg)l=p;
 if(foreground(texture(uDepth,uv+vec2(uTexel.x,0)).r)!=fg)r=p;
 if(foreground(texture(uDepth,uv-vec2(0,uTexel.y)).r)!=fg)d=p;
 if(foreground(texture(uDepth,uv+vec2(0,uTexel.y)).r)!=fg)u=p;
 vec3 dx=length(r-p)<1e-6?p-l:length(p-l)<1e-6?r-p:abs(r.z-p.z)<abs(p.z-l.z)?r-p:p-l;
 vec3 dy=length(u-p)<1e-6?p-d:length(p-d)<1e-6?u-p:abs(u.z-p.z)<abs(p.z-d.z)?u-p:p-d;
 vec3 n=cross(dx,dy);float nl=length(n);if(nl<1e-8){color=vec4(1,fg?p.z:-p.z,0,1);return;}n/=nl;if(dot(n,-p)<0)n=-n;
 float radiusPixels=min(uMaxPixels,uRadius/(max(.001,-p.z)*uInvProjection.y)*.5/uTexel.y);
 if(radiusPixels<1){color=vec4(1,fg?p.z:-p.z,0,1);return;}
 // Fixed spatial rotation: no random changes between frames or timescales.
 vec2 cell=mod(floor(gl_FragCoord.xy),4.0);float noise=fract(dot(cell,vec2(.754877666,.569840296)));
 float ao=0;
 for(int dir=0;dir<16;++dir){if(dir>=uDirections)break;
  float angle=6.28318530718*(float(dir)+noise)/float(uDirections);vec2 ray=vec2(cos(angle),sin(angle));
  // Tangent elevation sets the initial horizon; subsequent samples only
  // contribute when they raise it. Distance attenuation limits large halos.
  float tangent=atan(-dot(n.xy,ray),max(.001,n.z));float horizon=sin(clamp(tangent+uBias,-1.5707,1.5707));float baseline=horizon,occlusion=0;
  for(int stepIndex=1;stepIndex<=12;++stepIndex){if(stepIndex>uSteps)break;
   float pixels=max(1.0,(float(stepIndex)-.5+.5*noise)*radiusPixels/float(uSteps));vec2 qUv=uv+round(ray*pixels)*uTexel;
   if(outside(qUv)||texture(uDepth,qUv).r>=.9999999)continue;bool sampleFg=foreground(texture(uDepth,qUv).r);
   if(sampleFg!=fg&&!(uWeaponBackgroundHalo&&!fg&&sampleFg))continue;
   vec3 v=positionAt(qUv)-p;
   // Intentional old-school screen-space halo, not physical contact shadow:
   // project the foreground silhouette near the background receiver plane.
   if(sampleFg!=fg)v=vec3((qUv-uv)*2.0*uInvProjection*(-p.z),uRadius*.35);
   float distanceSquared=dot(v,v);
   if(distanceSquared<1e-6||distanceSquared>uRadius*uRadius)continue;
   float elevation=v.z*inversesqrt(distanceSquared);
   if(elevation>horizon){float attenuation=pow(max(0.0,1.0-distanceSquared/(uRadius*uRadius)),uFalloff);occlusion+=(elevation-horizon)*attenuation;horizon=elevation;}
  }
  ao+=occlusion/max(.1,1.0-baseline);
 }
 color=vec4(clamp(1.0-ao/float(uDirections),0,1),fg?p.z:-p.z,0,1);
}
)GLSL";
}
