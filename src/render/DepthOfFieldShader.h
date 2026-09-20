#pragma once
#include "render/AoFogShader.h"
namespace render {
inline constexpr const char* kDofFragment=R"GLSL(
#version 330 core
in vec2 vScreen;out vec4 color;
uniform sampler2D uSource,uDepth,uAo,uIsolated;
uniform bool uForeground,uAffectViewmodel,uAoEnabled,uIsolation;
uniform vec2 uTexel;
uniform vec2 uViewmodelDepthRange;
uniform float uNear,uFar,uFocus,uRange,uNearTransition,uFarTransition,uNearRadius,uFarRadius;
uniform float uGamma,uBokeh,uThreshold,uHollow,uAnamorphic,uRotation,uAoIntensity,uAoPower;
uniform int uSamples,uBlades,uMode;
)GLSL" CADENCE_AO_FOG_GLSL R"GLSL(
bool foreground(float raw){return uForeground&&raw<.0400001;}
float depthAt(vec2 uv){float z=texture(uDepth,uv).r;vec2 range=vec2(uNear,uFar);if(foreground(z)){z=clamp(z/.04,0,1);range=uViewmodelDepthRange;}return 2*range.x*range.y/(range.y+range.x-(z*2-1)*(range.y-range.x));}
float coc(vec2 uv){if(!uAffectViewmodel&&foreground(texture(uDepth,uv).r))return 0;float z=depthAt(uv);return clamp((z-uFocus-uRange*.5)/uFarTransition,0,1)*uFarRadius-clamp((uFocus-uRange*.5-z)/uNearTransition,0,1)*uNearRadius;}
vec3 source(vec2 uv){vec3 c=texture(uSource,uv).rgb;float ao=uAoEnabled?pow(clamp(1-(1-texture(uAo,uv).r)*uAoIntensity,0,1),uAoPower):1;c=c*ao+aoFogRestore(uv,ao);if(uIsolation)c+=texture(uIsolated,uv).rgb*(1-ao);return pow(max(c,vec3(0)),vec3(uGamma));}
void main(){vec2 uv=vScreen*.5+.5;float centerCoc=coc(uv),centerDepth=depthAt(uv);bool centerFg=foreground(texture(uDepth,uv).r);
 float radius=uMode==0?max(0,centerCoc):uNearRadius;
 if(radius<.5){color=vec4(pow(source(uv),vec3(1/uGamma)),0);return;}
 vec3 total=vec3(0);float weights=0,coverage=0,coverageWeights=0;
 for(int i=0;i<96;++i){if(i>=uSamples)break;float fraction=(float(i)+.5)/float(uSamples),angle=float(i)*2.39996323+uRotation;float ring=sqrt(mix(uHollow*uHollow,1.0,fraction));
  float shape=1;if(uBlades>=3){float sector=6.2831853/float(uBlades);shape=cos(3.14159265/float(uBlades))/cos(mod(angle-uRotation+sector*.5,sector)-sector*.5);}
  vec2 delta=vec2(cos(angle)*uAnamorphic,sin(angle)/uAnamorphic)*ring*radius*shape;vec2 q=clamp(uv+delta*uTexel,vec2(0),vec2(1));float sampleCoc=coc(q);bool sampleFg=foreground(texture(uDepth,q).r);
  float w=1;
  // Out-of-focus surfaces may overlap. Rejecting every nearer depth preserves
  // sharp silhouettes inside a blur. Instead test the source disc footprint;
  // focused/near surfaces and the separately rendered viewmodel stay protected.
  if(uMode==0){w=sampleCoc>0?smoothstep(-1.0,1.0,sampleCoc-ring*radius):0;if(centerFg!=sampleFg)w=0;}
  else {w=sampleCoc<0?smoothstep(-1.0,1.0,-sampleCoc-ring*radius):0;if(!uAffectViewmodel&&centerFg)w=0;}
  vec3 c=source(q);float bright=1+uBokeh*max(dot(c,vec3(.2126,.7152,.0722))-uThreshold,0)*8;
  total+=c*(w*bright);weights+=w*bright;coverage+=w;coverageWeights+=1;
 }
 vec3 blurred=weights>1e-5?pow(total/weights,vec3(1/uGamma)):pow(source(uv),vec3(1/uGamma));
 float alpha=uMode==0?smoothstep(.5,2.0,max(0,centerCoc)):clamp(coverage/max(1,coverageWeights),0,1);
 color=vec4(blurred,alpha);
}
)GLSL";
}
