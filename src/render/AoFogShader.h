#pragma once

// Shared by the sharp composite and DOF gather. Fog has already been drawn by
// the mesh pass: (surface * (1-f) + fog * f) * ao + fog * f * (1-ao)
// is surface * ao * (1-f) + fog * f. Do not merely fade the AO factor toward 1:
// that incorrectly restores occluded surface radiance as well as fog radiance.
// This is a screen-space reconstruction, using the opaque receiver depth for
// layered transparency (just as HBAO itself does), not volumetric per-layer AO.
#define CADENCE_AO_FOG_GLSL R"GLSL(
uniform bool uAoBeforeFog,uAoFogForeground,uAoFogHeightEnabled;
uniform sampler2D uAoFogDepth;
uniform vec2 uAoFogDepthRange,uAoFogInvProjection;
uniform vec3 uAoFogColor,uAoFogWorldZ;
uniform float uAoFogCameraZ,uAoFogStart,uAoFogHalfDistance,uAoFogOpacity,uAoFogHeight,uAoFogHeightFalloff;
vec3 aoFogRestore(vec2 uv,float ao){
    if(!uAoBeforeFog||ao>=1.0)return vec3(0);
    float raw=texture(uAoFogDepth,uv).r;
    // Sky and the separate first-person projection are not world receivers.
    if(raw>=.9999999||(uAoFogForeground&&raw<.0400001))return vec3(0);
    float n=uAoFogDepthRange.x,f=uAoFogDepthRange.y;
    float z=2.0*n*f/(f+n-(raw*2.0-1.0)*(f-n));
    vec3 ray=vec3((uv*2.0-1.0)*uAoFogInvProjection,1.0);
    float distanceVisibility=exp2(-max(0.0,z*length(ray)-uAoFogStart)/max(1.0,uAoFogHalfDistance));
    float worldZ=uAoFogCameraZ+z*dot(ray,uAoFogWorldZ);
    float heightDensity=uAoFogHeightEnabled?exp2(-max(0.0,worldZ-uAoFogHeight)/max(1.0,uAoFogHeightFalloff)):1.0;
    float amount=clamp((1.0-distanceVisibility)*heightDensity*clamp(uAoFogOpacity,0.0,1.0),0.0,1.0);
    return uAoFogColor*(amount*(1.0-ao));
}
)GLSL"
