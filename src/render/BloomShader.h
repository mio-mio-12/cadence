#pragma once
namespace render {
inline constexpr const char* kKawaseFragment=R"GLSL(
#version 330 core
in vec2 vScreen;out vec4 color;
uniform sampler2D uSource;uniform vec2 uTexel;uniform float uOffset;
uniform bool uThreshold;uniform float uThresholdValue,uThresholdSoftness,uSaturationBias,uAspect,uRotation;
// A bad source pixel must not contaminate the entire down/up-sampling pyramid.
// Sanitize before averaging; retain every representable finite HDR value.
vec3 bloomFinite(vec3 v){
    for(int i=0;i<3;++i)v[i]=isnan(v[i])||isinf(v[i])?0.0:clamp(v[i],-65504.0,65504.0);
    return v;
}
vec3 bloomTap(vec2 uv){return bloomFinite(texture(uSource,uv).rgb);}
void main(){
    vec2 uv=vScreen*.5+.5;float c=cos(uRotation),s=sin(uRotation);
    vec2 base=vec2(uTexel.x*max(.01,uAspect),uTexel.y/max(.01,uAspect))*uOffset;
    vec2 o=vec2(base.x*c-base.y*s,base.x*s+base.y*c);
    vec3 rgb=(bloomTap(uv-o)+bloomTap(uv+vec2(o.x,-o.y))+bloomTap(uv+vec2(-o.x,o.y))+bloomTap(uv+o))*.25;
    if(uThreshold){
        float l=max(rgb.r,max(rgb.g,rgb.b));
        float softness=max(0.,uThresholdSoftness);
        float weight=softness>0.?smoothstep(uThresholdValue-softness,uThresholdValue+softness,l):step(uThresholdValue,l);
        rgb*=weight;float gray=dot(rgb,vec3(.2126,.7152,.0722));
        rgb=mix(vec3(gray),rgb,max(0.,1.+uSaturationBias));
    }
    color=vec4(bloomFinite(rgb),1);
}
)GLSL";
}
