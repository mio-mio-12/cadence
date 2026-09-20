#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

#include "render/StageRenderer.h"
#include "render/ForegroundDepth.h"
#include "render/DayNightSky.h"
#include "render/MeshUniformCache.h"
#include "render/HbaoShader.h"
#include "render/VolumetricLightingShader.h"
#include "render/WaterShader.h"
#include "render/AoFogShader.h"
#include "render/DepthOfFieldShader.h"
#include "render/StaticMaterialOrder.h"
#include "render/TexturePixels.h"
#include "render/PreparedTexture.h"
#include "render/BoundedPreparationQueue.h"
#include <atomic>
#include <chrono>
#include "render/GlApi.h"
#include "render/DdsLoader.h"
#include "scene/TestCourse.h"
#include "take/DollyCamera.h"
#include <GLFW/glfw3.h>
#include <webp/decode.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace render {
namespace {

const char* kMeshVertex = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUv;
layout(location=3) in uvec4 aBones;
layout(location=4) in vec4 aWeights;
layout(location=5) in vec4 aColor;
uniform mat4 uViewProjection;
uniform vec2 uForegroundClip;
uniform float uOverlayExpand;
uniform vec2 uOverlayViewport;
uniform mat4 uModel;
uniform samplerBuffer uBones;
uniform samplerBuffer uBoneVisibility;
uniform bool uSkinned;
uniform mat4 uLightViewProjection;
uniform mat4 uFarLightViewProjection;
uniform mat4 uViewmodelLightViewProjection;
out vec3 vNormal;
out vec2 vUv;
out float vVisible;
out vec4 vLightPosition;
out vec4 vFarLightPosition;
out vec4 vViewmodelLightPosition;
out vec3 vWorldPosition;
out vec4 vColor;
mat4 boneMatrix(uint index) {
    int base=int(index)*4;
    return mat4(texelFetch(uBones,base),texelFetch(uBones,base+1),texelFetch(uBones,base+2),texelFetch(uBones,base+3));
}
void main() {
    mat4 skin=mat4(1.0);
    if(uSkinned) skin=boneMatrix(aBones.x)*aWeights.x+boneMatrix(aBones.y)*aWeights.y+
                      boneMatrix(aBones.z)*aWeights.z+boneMatrix(aBones.w)*aWeights.w;
    vVisible=uSkinned?(texelFetch(uBoneVisibility,int(aBones.x)).r*aWeights.x+texelFetch(uBoneVisibility,int(aBones.y)).r*aWeights.y+
                       texelFetch(uBoneVisibility,int(aBones.z)).r*aWeights.z+texelFetch(uBoneVisibility,int(aBones.w)).r*aWeights.w):1.0;
    vec4 world=uModel*skin*vec4(aPosition,1.0);
    vNormal=normalize(mat3(uModel*skin)*aNormal);
    // WIC copies PNG rows top-to-bottom while GL treats the first uploaded row
    // as v=0. CoD Cast UVs also use a top-left origin, so the upload already
    // accounts for orientation; flipping V here selects the wrong atlas parts.
    vUv=aUv;
    vLightPosition=uLightViewProjection*world;
    vFarLightPosition=uFarLightViewProjection*world;
    vViewmodelLightPosition=uViewmodelLightViewProjection*world;
    vWorldPosition=world.xyz;
    vColor=aColor;
    gl_Position=uViewProjection*world;
    // Only Z changes: authored pose, lens size, FOV and X/Y projection stay exact.
    // World geometry never uses this close-range projection.
    if(uForegroundClip.x>0.0){float n=uForegroundClip.x,f=uForegroundClip.y;gl_Position.z=((f+n)*gl_Position.w-2.0*f*n)/(f-n);}
    if(uOverlayExpand>0.0){vec2 d=(uViewProjection*vec4(vNormal,0)).xy;gl_Position.xy+=d/max(length(d),.00001)*uOverlayExpand*2.0/max(uOverlayViewport,vec2(1))*gl_Position.w;}
}
)GLSL";

const char* kMeshFragment = R"GLSL(
#version 330 core
in vec3 vNormal;
in vec2 vUv;
in float vVisible;
in vec4 vLightPosition;
in vec4 vFarLightPosition;
in vec4 vViewmodelLightPosition;
in vec3 vWorldPosition;
in vec4 vColor;
uniform vec4 uColor;
uniform int uActorOverlay;
uniform vec4 uActorOverlayColor;
uniform float uWorldOpacity;
uniform sampler2D uAlbedo;
uniform bool uAoIsolation,uAoHasForeground;
uniform sampler2D uAoOpaqueDepth;
uniform vec2 uAoDepthRange;
uniform float uAoGap;
uniform bool uHasAlbedo;
uniform sampler2D uNormalMap;
uniform bool uHasNormalMap;
uniform float uNormalIntensity;
uniform sampler2D uSpecularMap;
uniform bool uHasSpecularMap;
uniform sampler2D uMetalnessMap;
uniform bool uHasMetalnessMap;
uniform float uWeaponMetalnessOverride;
uniform vec4 uWeaponMetalnessDiffuse;
uniform bool uSource2Material;
uniform sampler2D uRoughnessMap;
uniform bool uHasRoughnessMap;
)GLSL" R"GLSL(
uniform bool uLens;
uniform bool uEyeOverlay;
uniform bool uEmissive;
uniform bool uExplicitMaterial;
uniform bool uUnlit;
uniform bool uVertexColor;
uniform bool uHasEmissionMap;
uniform float uAlphaCutoff;
uniform vec3 uEmissiveTint;
uniform bool uForceAlpha;
uniform bool uDecal;
uniform bool uDecalMultiply;
uniform bool uDecalAdditive;
uniform bool uAlphaTest;
uniform sampler2D uCamo;
uniform bool uHasCamo;
uniform bool uCamoBlend;
uniform bool uIgnoreAlpha;
uniform float uCamoStrength;
uniform float uCamoScale;
uniform vec2 uCamoOffset;
uniform float uCamoRotation;
uniform float uCamoAlphaLow;
uniform float uCamoAlphaHigh;
uniform bool uCamoInvert;
uniform bool uCamoUseAlpha;
uniform bool uCamoMaskUseful;
uniform bool uCamoLumaMask;uniform float uCamoLumaLow;uniform float uCamoLumaHigh;uniform float uCamoLumaGamma;uniform float uCamoLumaContrast;
uniform float uLensAlpha;
uniform vec3 uLensTint;
uniform float uLensTintIntensity;
uniform float uLensCubemapIntensity;
uniform float uSpecularIntensity;
uniform float uSpecularSharpness;
uniform float uSpecularIntensity2;
uniform float uSpecularSharpness2;
uniform float uLensSpecularIntensity;
uniform bool uWeaponSurface;
uniform float uWeaponSpecularMultiplier;
uniform vec3 uWeaponSpecularLow;
uniform vec3 uWeaponSpecularHigh;
uniform float uWeaponCubemapMultiplier;
uniform float uWeaponCubemapBlur;
uniform float uSkyRotation;
uniform float uNormalReflectionInfluence;
uniform bool uCubemapSpecular;
uniform bool uHasSpecCubemap;
uniform float uCubemapSpecularIntensity;
uniform vec2 uCubemapSurfaceMultipliers;
uniform float uCubemapBlur;
uniform bool uSkyFlipVertical;
uniform int uSkyMode;
uniform float uSkyIntensity;
uniform float uSkyExposure;
uniform float uSkyContrast;
uniform float uSkyHighlightBoost;
uniform float uSkyHighlightThreshold;
uniform float uSkyToneMap;
// Panorama and front-face modes share uSpecFront; only one is active.
uniform bool uAlphaOverlay;
uniform float uAlphaOverlayIntensity;
uniform int uDebugView;
uniform int uShadingModel;
uniform int uBrdfModel;
uniform bool uIw3DualLobe;
uniform float uAwRoughnessScale;uniform float uAwRoughnessBias;uniform float uAwMetalness;uniform float uAwSpecularLevel;uniform float uAwDiffuseWrap;uniform float uAwClearcoat;uniform float uAwClearcoatRoughness;uniform float uAwEnvironmentIntensity;
uniform float uEeveeMetallic;uniform float uEeveeRoughness;uniform float uEeveeIor;uniform float uEeveeSpecular;uniform float uEeveeSpecularTint;uniform float uEeveeClearcoat;uniform float uEeveeClearcoatRoughness;
uniform bool uFilmEnabled;uniform float uFilmBrightness;uniform float uFilmContrast;uniform float uFilmDesaturation;uniform vec3 uFilmDarkTint;uniform vec3 uFilmMidTint;uniform vec3 uFilmLightTint;uniform bool uFilmMidTintEnabled;uniform bool uFilmInvert;uniform bool uBloomEnabled;uniform float uBloomThreshold;uniform float uBloomIntensity;uniform bool uVignetteEnabled;uniform float uVignetteIntensity;uniform float uVignetteRadius;uniform float uVignetteSoftness;uniform vec2 uViewportSize;
uniform bool uFogEnabled;uniform vec3 uFogColor;uniform float uFogStart;uniform float uFogHalfDistance;uniform float uFogOpacity;uniform bool uFogHeightEnabled;uniform float uFogHeight;uniform float uFogHeightFalloff;
uniform sampler2D uSpecularImperfections;
uniform bool uHasSpecularImperfections;
uniform float uImperfectionStrength;
uniform float uImperfectionScale;
uniform float uImperfectionLow;
uniform float uImperfectionHigh;
uniform sampler2D uSpecFront;uniform sampler2D uSpecBack;uniform sampler2D uSpecLeft;uniform sampler2D uSpecRight;uniform sampler2D uSpecUp;uniform sampler2D uSpecDown;
uniform int uSpecFaceSource0;uniform int uSpecFaceSource1;uniform int uSpecFaceSource2;uniform int uSpecFaceSource3;uniform int uSpecFaceSource4;uniform int uSpecFaceSource5;
uniform int uSpecFaceRotation0;uniform int uSpecFaceRotation1;uniform int uSpecFaceRotation2;uniform int uSpecFaceRotation3;uniform int uSpecFaceRotation4;uniform int uSpecFaceRotation5;
uniform bool uSunEnabled;
uniform bool uShadowEnabled;
uniform vec3 uSunDirection;
uniform float uSunIntensity;
uniform float uSunAmbient;
uniform vec3 uSunColor;
uniform vec3 uAmbientColor;
uniform vec3 uCameraPosition;
uniform float uShadowDistance;
uniform float uShadowFadeStart;
uniform float uShadowIntensity;
uniform float uShadowBias;
uniform float uShadowNormalBias;
uniform float uShadowSoftness;
uniform float uShadowContrast;
uniform sampler2D uShadowMap;
uniform sampler2D uViewmodelShadowMap;
uniform bool uViewmodelSurface;
uniform bool uViewmodelSelfShadows;
uniform bool uGltfPbr;
uniform bool uSpecularGlossiness;
uniform float uGltfMetallic;
uniform float uGltfRoughness;
uniform float uGltfTransmission;
uniform float uGltfIor;
uniform vec3 uGltfEmissive;
uniform sampler2D uFarShadowMap;
uniform bool uFarShadowEnabled;
uniform float uFarShadowStart;
uniform float uFarShadowDistance;
uniform float uFarShadowBlend;
uniform float uShadowSpecularMultiplier;
out vec4 color;
float sampleShadowMap(sampler2D shadowMap,vec4 lightPosition,vec3 normal){
    vec3 projected=lightPosition.xyz/lightPosition.w*0.5+0.5;
    if(projected.z<=0.0||projected.z>=1.0||projected.x<=0.0||projected.x>=1.0||projected.y<=0.0||projected.y>=1.0)return 1.0;
    float bias=uShadowBias+uShadowNormalBias*0.01*(1.0-dot(normalize(normal),normalize(-uSunDirection)));
    vec2 texel=1.0/vec2(textureSize(shadowMap,0));float visible=0.0;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)visible+=(projected.z-bias<=texture(shadowMap,projected.xy+vec2(x,y)*texel*uShadowSoftness).r)?1.0:0.0;
    return visible/9.0;
}
float shadowVisibility(vec3 normal){
    if(!uShadowEnabled)return 1.0;float distanceToCamera=length(vWorldPosition-uCameraPosition);float nearShadow=sampleShadowMap(uShadowMap,vLightPosition,normal),worldVisibility;if(!uFarShadowEnabled)worldVisibility=mix(1.0,nearShadow,1.0-smoothstep(uShadowFadeStart,uShadowDistance,distanceToCamera));else {float blend=max(0.001,uFarShadowBlend),nearWeight=1.0-smoothstep(uFarShadowStart-blend,uFarShadowStart+blend,distanceToCamera),farWeight=smoothstep(uFarShadowStart-blend,uFarShadowStart+blend,distanceToCamera)*(1.0-smoothstep(uFarShadowDistance-blend,uFarShadowDistance,distanceToCamera));float farShadow=farWeight>0.001?sampleShadowMap(uFarShadowMap,vFarLightPosition,normal):1.0;worldVisibility=mix(1.0,nearShadow,nearWeight)*(1.0-farWeight)+farShadow*farWeight;}if(uViewmodelSurface&&uViewmodelSelfShadows)worldVisibility*=sampleShadowMap(uViewmodelShadowMap,vViewmodelLightPosition,normal);worldVisibility=clamp((worldVisibility-.5)*uShadowContrast+.5,0.0,1.0);return mix(1.0,worldVisibility,clamp(uShadowIntensity,0.0,1.0));
}
mat3 cotangentFrame(vec3 normal,vec3 position,vec2 uv){vec3 dp1=dFdx(position),dp2=dFdy(position);vec2 duv1=dFdx(uv),duv2=dFdy(uv);vec3 dp2perp=cross(dp2,normal),dp1perp=cross(normal,dp1);vec3 tangent=dp2perp*duv1.x+dp1perp*duv2.x,bitangent=dp2perp*duv1.y+dp1perp*duv2.y;float scale=inversesqrt(max(dot(tangent,tangent),dot(bitangent,bitangent)));return mat3(tangent*scale,bitangent*scale,normal);}
int specFaceSource(int face){if(face==0)return uSpecFaceSource0;if(face==1)return uSpecFaceSource1;if(face==2)return uSpecFaceSource2;if(face==3)return uSpecFaceSource3;if(face==4)return uSpecFaceSource4;return uSpecFaceSource5;}
int specFaceRotation(int face){if(face==0)return uSpecFaceRotation0;if(face==1)return uSpecFaceRotation1;if(face==2)return uSpecFaceRotation2;if(face==3)return uSpecFaceRotation3;if(face==4)return uSpecFaceRotation4;return uSpecFaceRotation5;}
vec2 specRotate(vec2 uv,int turns){int q=((turns%4)+4)%4;if(q==1)return vec2(1.0-uv.y,uv.x);if(q==2)return vec2(1.0-uv.x,1.0-uv.y);if(q==3)return vec2(uv.y,1.0-uv.x);return uv;}
vec3 specRaw(int source,vec2 uv){if(source==0)return texture(uSpecFront,uv).rgb;if(source==1)return texture(uSpecBack,uv).rgb;if(source==2)return texture(uSpecLeft,uv).rgb;if(source==3)return texture(uSpecRight,uv).rgb;if(source==4)return texture(uSpecUp,uv).rgb;return texture(uSpecDown,uv).rgb;}
vec3 specLogical(int face,vec2 uv){return specRaw(specFaceSource(face),specRotate(uv,specFaceRotation(face)));}
vec3 specLogicalSample(vec3 d){vec3 a=abs(d);vec2 q;if(a.x>=a.y&&a.x>=a.z){if(d.x>=0.0){q=vec2(-d.z,-d.y)/a.x;return specLogical(0,vec2(q.x,-q.y)*.5+.5);}q=vec2(d.z,-d.y)/a.x;return specLogical(1,vec2(q.x,-q.y)*.5+.5);}if(a.y>=a.z){if(d.y>=0.0){q=vec2(d.x,d.z)/a.y;return specLogical(2,vec2(q.x,-q.y)*.5+.5);}q=vec2(d.x,-d.z)/a.y;return specLogical(3,vec2(q.x,-q.y)*.5+.5);}if(d.z>=0.0){q=vec2(d.x,-d.y)/a.z;return specLogical(4,vec2(q.x,-q.y)*.5+.5);}q=vec2(-d.x,-d.y)/a.z;return specLogical(5,vec2(q.x,-q.y)*.5+.5);}
vec3 sampleSpecCube(vec3 worldDirection){float c=cos(uSkyRotation),s=sin(uSkyRotation);vec3 rd=vec3(worldDirection.x*c-worldDirection.y*s,worldDirection.x*s+worldDirection.y*c,worldDirection.z);vec3 d=vec3(rd.x,-rd.y,rd.z);if(uSkyFlipVertical)d.z=-d.z;vec3 rgb;if(uSkyMode==1){vec2 uv=vec2(atan(d.y,d.x)/6.2831853+.5,asin(clamp(d.z,-1.0,1.0))/3.14159265+.5);rgb=texture(uSpecFront,uv).rgb;}else rgb=specLogicalSample(d);rgb*=uSkyIntensity*exp2(uSkyExposure);rgb=max(vec3(0),.5+(rgb-.5)*uSkyContrast);float l=max(rgb.r,max(rgb.g,rgb.b)),highlight=smoothstep(uSkyHighlightThreshold,max(uSkyHighlightThreshold+.001,1.0),l);rgb*=mix(1.0,uSkyHighlightBoost,highlight);rgb=mix(rgb,rgb/(rgb+vec3(1)),clamp(uSkyToneMap,0.0,1.0));return rgb;}
uniform int uCubemapSamples;
vec3 sampleSpecCubeDualKawase(vec3 direction,float blur){direction=normalize(direction);if(blur<=0.001)return sampleSpecCube(direction);vec3 helper=abs(direction.z)<0.85?vec3(0,0,1):vec3(0,1,0);vec3 tangent=normalize(cross(helper,direction)),bitangent=normalize(cross(direction,tangent));vec3 result=sampleSpecCube(direction)*0.20;float totalWeight=0.20;int samples=clamp(uCubemapSamples,4,32);float maxR=blur*1.6;for(int i=0;i<32;++i){if(i>=samples)break;float fi=float(i)+0.5;float r=sqrt(fi/float(samples))*maxR;float theta=float(i)*2.3999632;float w=exp(-3.0*(r/maxR)*(r/maxR));vec3 sampleDir=normalize(direction+(tangent*cos(theta)+bitangent*sin(theta))*r);result+=sampleSpecCube(sampleDir)*w;totalWeight+=w;}return result/totalWeight;}
float schlickSmith(float ndl,float ndv,float exponent){float roughness=sqrt(2.0/(max(2.0,exponent)+2.0)),k=roughness*roughness*0.5;float gl=ndl/max(0.0001,ndl*(1.0-k)+k),gv=ndv/max(0.0001,ndv*(1.0-k)+k);return gl*gv;}
vec3 codSpecularLobe(vec3 f0,float exponent,float intensity,vec3 n,vec3 l,vec3 v){vec3 h=normalize(l+v);float ndl=max(dot(n,l),0.0),ndv=max(dot(n,v),0.0),ndh=max(dot(n,h),0.0),vdh=max(dot(v,h),0.0);vec3 fresnel=f0+(vec3(1.0)-f0)*pow(1.0-vdh,5.0);float normalized=(exponent+2.0)/6.2831853;return fresnel*(normalized*pow(ndh,exponent)*schlickSmith(ndl,ndv,exponent)*ndl)*intensity;}
vec3 awSpecular(vec3 f0,float roughness,vec3 n,vec3 l,vec3 v){vec3 h=normalize(l+v);float ndl=max(dot(n,l),0.0),ndv=max(dot(n,v),0.0),ndh=max(dot(n,h),0.0),vdh=max(dot(v,h),0.0),a=max(.025,roughness*roughness),a2=a*a,d=a2/(3.14159265*pow(max(.0001,ndh*ndh*(a2-1.0)+1.0),2.0)),k=pow(roughness+1.0,2.0)/8.0,g=(ndl/(ndl*(1.0-k)+k))*(ndv/(ndv*(1.0-k)+k));vec3 f=f0+(vec3(1)-f0)*pow(1.0-vdh,5.0);return f*d*g*ndl/max(.001,4.0*ndl*ndv);}
)GLSL" R"GLSL(
vec3 film(vec3 rgb){if(!uFilmEnabled)return rgb;float l=dot(rgb,vec3(.2126,.7152,.0722));rgb=mix(rgb,vec3(l),clamp(uFilmDesaturation,-1.0,1.0));rgb=(rgb-.5)*uFilmContrast+.5+uFilmBrightness;vec3 tint=mix(uFilmDarkTint,uFilmLightTint,clamp(l,0.0,1.0));if(uFilmMidTintEnabled)tint=mix(tint,uFilmMidTint,clamp(1.0-abs(l*2.0-1.0),0.0,1.0));rgb*=tint;if(uBloomEnabled){vec3 bright=max(rgb-vec3(uBloomThreshold),vec3(0));rgb+=bright*bright/(bright+vec3(.25))*uBloomIntensity;}if(uVignetteEnabled){vec2 uv=gl_FragCoord.xy/max(uViewportSize,vec2(1));float d=length((uv-.5)*vec2(uViewportSize.x/uViewportSize.y,1));float v=smoothstep(uVignetteRadius,max(uVignetteRadius+.001,uVignetteRadius+uVignetteSoftness),d);rgb*=1.0-v*clamp(uVignetteIntensity,0.0,1.0);}if(uFilmInvert)rgb=vec3(1)-rgb;return max(rgb,vec3(0));}
void main() {
    if(vVisible<0.5)discard;
    if(uActorOverlay>0){
        if(uAlphaTest&&uHasAlbedo&&texture(uAlbedo,vUv).a<uAlphaCutoff)discard;
        vec3 n=normalize(vNormal),v=normalize(uCameraPosition-vWorldPosition);
        float f=pow(1.0-abs(dot(n,v)),3.0);
        vec3 c=uActorOverlayColor.rgb;float a=uActorOverlayColor.a;
        if(uActorOverlay==2){vec3 env=uHasSpecCubemap?sampleSpecCubeDualKawase(reflect(-v,n),.12):vec3(.15)+vec3(.85)*pow(abs(dot(n,normalize(vec3(.3,.5,1)))),12.0);c=mix(c*.15,env*c*1.8,.85)+f*.3;}
        if(uActorOverlay==4){c*=.25+1.5*f;a*=.12+.88*f;}
        color=vec4(c,a);return;
    }
    if(uAoIsolation){float behind=texture(uAoOpaqueDepth,gl_FragCoord.xy/uViewportSize).r;if(uAoHasForeground&&behind<.0400001)discard;float n=uAoDepthRange.x,f=uAoDepthRange.y;float zb=2*n*f/(f+n-(behind*2-1)*(f-n)),zf=2*n*f/(f+n-(gl_FragCoord.z*2-1)*(f-n));if(zb-zf<uAoGap)discard;}
    // Corneal shells transmit the iris underneath; their color maps are not
    // diffuse paint. Render only dielectric reflection, with premultiplied
    // coverage, independently of weapon-glass tint/opacity controls.
    if(uEyeOverlay){
        vec3 n=normalize(vNormal),v=normalize(uCameraPosition-vWorldPosition);
        float fresnel=0.025+0.975*pow(1.0-clamp(dot(n,v),0.0,1.0),5.0);
        vec3 reflection=vec3(0.0);
        if(uSunEnabled)reflection+=awSpecular(vec3(0.025),0.08,n,normalize(-uSunDirection),v)*uSunColor*uSunIntensity*shadowVisibility(n);
        if(uCubemapSpecular&&uHasSpecCubemap)reflection+=sampleSpecCubeDualKawase(reflect(-v,n),0.025)*fresnel*uCubemapSpecularIntensity*(uViewmodelSurface?uCubemapSurfaceMultipliers.x:uCubemapSurfaceMultipliers.y);
        float fogVisibility=1.0;
        if(uFogEnabled){
            float distanceVisibility=exp2(-max(0.0,length(vWorldPosition-uCameraPosition)-uFogStart)/max(1.0,uFogHalfDistance));
            float heightDensity=uFogHeightEnabled?exp2(-max(0.0,vWorldPosition.z-uFogHeight)/max(1.0,uFogHeightFalloff)):1.0;
            fogVisibility=1.0-clamp((1.0-distanceVisibility)*heightDensity*clamp(uFogOpacity,0.0,1.0),0.0,1.0);
        }
        color=vec4(film(reflection/max(fresnel,0.001))*fresnel*fogVisibility,fresnel*fogVisibility);
        return;
    }
    vec4 base=uHasAlbedo?texture(uAlbedo,vUv):uColor;if(uExplicitMaterial&&uHasAlbedo)base*=uColor;if(uEmissive&&!uGltfPbr){float emissiveValue=max(base.r,max(base.g,base.b));base.rgb=uEmissiveTint*emissiveValue;}
    vec4 rawNormalSample=uHasNormalMap?texture(uNormalMap,vUv):vec4(0.5,0.5,1.0,1.0);vec3 rawNormal=rawNormalSample.xyz,decodedNormal=rawNormal*2.0-1.0;decodedNormal.xy*=uNormalIntensity;vec3 normal=normalize(vNormal);if(uExplicitMaterial&&!gl_FrontFacing)normal=-normal;if(uHasNormalMap)normal=normalize(cotangentFrame(normal,vWorldPosition,vUv)*decodedNormal);
    bool overrideMetal=uWeaponSurface&&!uLens&&uWeaponMetalnessOverride>=0.0;bool source2Pbr=(uSource2Material&&uHasMetalnessMap)||overrideMetal;vec4 specularSample=uHasSpecularMap?texture(uSpecularMap,vUv):(uSpecularGlossiness?vec4(.04,.04,.04,.5):vec4(base.a,base.a,base.a,base.a));float diffuseLuma=dot(base.rgb,vec3(.2126,.7152,.0722));float metalSpan=uWeaponMetalnessDiffuse.z-uWeaponMetalnessDiffuse.y;float diffuseMetal=abs(metalSpan)<.0001?step(uWeaponMetalnessDiffuse.y,diffuseLuma):clamp((diffuseLuma-uWeaponMetalnessDiffuse.y)/metalSpan,0.0,1.0);diffuseMetal=pow(diffuseMetal,max(.05,uWeaponMetalnessDiffuse.w));float source2Metal=overrideMetal?(uWeaponMetalnessDiffuse.x>.5?diffuseMetal:uWeaponMetalnessOverride):(uHasMetalnessMap?texture(uMetalnessMap,vUv).r:0.0);float source2Rough=uHasRoughnessMap?texture(uRoughnessMap,vUv).r:(1.0-specularSample.a);if(overrideMetal||((uWeaponSurface||source2Pbr)&&uHasMetalnessMap&&!uHasSpecularMap)){specularSample.rgb=mix(vec3(0.04),base.rgb,source2Metal);specularSample.a=1.0-source2Rough;}if(uWeaponSurface&&(uHasSpecularMap||uHasMetalnessMap||overrideMetal)){vec3 span=uWeaponSpecularHigh-uWeaponSpecularLow;vec3 safeSpan=mix(sign(span)*max(abs(span),vec3(.0001)),vec3(1),lessThan(abs(span),vec3(.0001)));specularSample.rgb=clamp((specularSample.rgb-uWeaponSpecularLow)/safeSpan,vec3(0),vec3(1))*uWeaponSpecularMultiplier;}if(uHasSpecularImperfections){float imperfection=texture(uSpecularImperfections,fract(vUv*uImperfectionScale)).r;imperfection=smoothstep(uImperfectionLow,max(uImperfectionLow+0.001,uImperfectionHigh),imperfection);float impFactor=mix(1.0,imperfection,clamp(uImperfectionStrength,0.0,1.0));specularSample.rgb*=impFactor;specularSample.a*=impFactor;}
    vec3 vertTint=(uExplicitMaterial?uVertexColor:uDecal)?vColor.rgb:vec3(1.0);float vertAlpha=(uExplicitMaterial?uVertexColor:uDecal)?vColor.a:1.0;vec3 rgb=base.rgb*vertTint*(uLens?mix(vec3(1.0),uLensTint,clamp(uLensTintIntensity,0.0,4.0)):vec3(1));float alpha=uLens?uLensAlpha:((uForceAlpha||uDecal||uAlphaTest)?base.a*vertAlpha*(uExplicitMaterial?1.0:uColor.a):1.0);float gltfMetal=clamp(uGltfMetallic*(uHasSpecularMap?specularSample.b:1.0),0.0,1.0),gltfRough=clamp(uGltfRoughness*(uHasSpecularMap?specularSample.g:1.0),0.025,1.0);if(overrideMetal)gltfMetal=source2Metal;vec3 gltfF0=mix(vec3(0.04),rgb,gltfMetal);
    if(uCamoBlend){float sourceMask=uCamoLumaMask?dot(base.rgb,vec3(.2126,.7152,.0722)):((uCamoUseAlpha&&uCamoMaskUseful)?base.a:1.0);if(uCamoLumaMask){sourceMask=pow(clamp(sourceMask,0.0,1.0),max(.05,uCamoLumaGamma));sourceMask=(sourceMask-.5)*uCamoLumaContrast+.5;float span=uCamoLumaHigh-uCamoLumaLow;sourceMask=abs(span)<.0001?step(uCamoLumaLow,sourceMask):clamp((sourceMask-uCamoLumaLow)/span,0.0,1.0);}float mask=uCamoInvert?1.0-sourceMask:sourceMask;if(!uCamoLumaMask){float span=uCamoAlphaHigh-uCamoAlphaLow;mask=abs(span)<.0001?step(uCamoAlphaLow,mask):clamp((mask-uCamoAlphaLow)/span,0.0,1.0);}float cr=cos(uCamoRotation),cs=sin(uCamoRotation);vec2 cuv=mat2(cr,-cs,cs,cr)*(vUv-.5);if(uHasCamo)rgb=mix(rgb,texture(uCamo,fract(cuv*uCamoScale+.5+uCamoOffset)).rgb,clamp(mask*uCamoStrength,0.0,1.0));if(uIgnoreAlpha)alpha=1.0;}
    if(uExplicitMaterial&&!uForceAlpha&&!uAlphaTest)alpha=1.0;
    if(!uLens&&(uForceAlpha||uDecal)&&alpha<0.005)discard;
    if(!uExplicitMaterial&&uDecal&&!uDecalMultiply&&!uDecalAdditive)alpha=smoothstep(0.015,0.055,alpha)*alpha;
    if(uDecalAdditive&&alpha<0.005)discard;
    if(!uLens&&uAlphaTest&&alpha<uAlphaCutoff)discard;
    if(uExplicitMaterial&&uAlphaTest)alpha=1.0;
    vec3 viewDirection=normalize(uCameraPosition-vWorldPosition);
    vec3 f0_eevee_local=vec3(0.04);
    float visibility=1.0;
    if(uUnlit){
        // Authored unlit keeps base artwork; zero emission stays zero.
    }else if(uEmissive){
        if(uGltfPbr){
            vec3 emitFactor=(length(uGltfEmissive)>0.001)?uGltfEmissive:vec3(1.0);
            if(uHasAlbedo){
                rgb=base.rgb*emitFactor;
            }else{
                rgb=(length(uGltfEmissive)>0.001)?uGltfEmissive:base.rgb;
            }
        }else{
            rgb=base.rgb*2.0;
        }
    }else if(uSunEnabled){
        vec3 lightDirection=normalize(-uSunDirection);
        float rawNdl=dot(normal,lightDirection),diffuse=max(rawNdl,0.0);
        visibility=shadowVisibility(normal);
        if(uShadingModel==2&&!uGltfPbr)diffuse=clamp((rawNdl+uAwDiffuseWrap)/(1.0+uAwDiffuseWrap),0.0,1.0);
        vec3 lighting=uAmbientColor*uSunAmbient+uSunColor*diffuse*uSunIntensity*visibility;
        float metalness=uGltfPbr?gltfMetal:(source2Pbr?source2Metal:(uSpecularGlossiness?0.0:clamp(uAwMetalness,0.0,1.0)));
        vec3 mappedF0=clamp(specularSample.rgb*uAwSpecularLevel,vec3(0.01),vec3(0.95));
        vec3 f0=uGltfPbr?gltfF0:(uLens?vec3(0.10):(uShadingModel==2?mix(mappedF0,rgb,metalness):mappedF0));
        float gloss1=mix(max(2.0,uSpecularSharpness*0.0625),uSpecularSharpness,uLens?1.0:((uHasSpecularMap||uHasRoughnessMap||uSpecularGlossiness)?specularSample.a:base.a));
        float gloss2=mix(max(2.0,uSpecularSharpness2*0.0625),uSpecularSharpness2,uLens?1.0:((uHasSpecularMap||uHasRoughnessMap||uSpecularGlossiness)?specularSample.a:base.a));
        float roughness=uLens?0.025:(uGltfPbr?gltfRough:clamp((1.0-((uHasSpecularMap||uHasRoughnessMap||uSpecularGlossiness)?specularSample.a:base.a))*uAwRoughnessScale+uAwRoughnessBias,.04,1.0));
        vec3 directSpec=vec3(0.0);
        if(uGltfPbr){
            directSpec=awSpecular(f0,roughness,normal,lightDirection,viewDirection)*uSpecularIntensity;
        }else if(uSpecularGlossiness&&!uLens&&!overrideMetal){
            // Native CODM exports contain linear specular color and smoothness,
            // not a legacy Phong exponent mask. Honor their roughness response.
            directSpec=awSpecular(f0,roughness,normal,lightDirection,viewDirection)*uSpecularIntensity;
        }else if(uShadingModel==0){
            vec3 reflected=reflect(-lightDirection,normal);
            directSpec=f0*pow(max(dot(reflected,viewDirection),0.0),max(2.0,gloss1))*uSpecularIntensity*diffuse;
            if(uIw3DualLobe)directSpec+=f0*pow(max(dot(reflected,viewDirection),0.0),max(2.0,gloss2))*uSpecularIntensity2*diffuse;
        }else if(uShadingModel==2){
            directSpec=awSpecular(f0,roughness,normal,lightDirection,viewDirection)*(uLens?uLensSpecularIntensity:uSpecularIntensity);
            directSpec+=awSpecular(vec3(0.04),clamp(uAwClearcoatRoughness,.04,1.0),normal,lightDirection,viewDirection)*uAwClearcoat;
        }else if(uShadingModel==3){
            float rough=clamp(uHasSpecularMap?(1.0-specularSample.a):uEeveeRoughness,0.025,1.0);
            float alpha_g=rough*rough;
            float metal=uSpecularGlossiness?0.0:clamp(uEeveeMetallic,0.0,1.0);
            float f0_d=pow((uEeveeIor-1.0)/max(0.001,uEeveeIor+1.0),2.0)*uEeveeSpecular*2.0;
            vec3 f0_eevee=clamp(mix(vec3(clamp(f0_d,0.0,1.0)),rgb,metal),vec3(0.0),vec3(1.0));
            f0_eevee_local=f0_eevee;
            if(uSpecularGlossiness){f0_eevee=clamp(specularSample.rgb,vec3(0),vec3(.99));f0_eevee_local=f0_eevee;}
)GLSL" R"GLSL(
            vec3 h=normalize(lightDirection+viewDirection);
            float ndh=max(dot(normal,h),0.0),ndv=max(dot(normal,viewDirection),0.0),vdh=max(dot(viewDirection,h),0.0);
            float clNdl=max(rawNdl,0.0);
            if(clNdl>0.0){
                float a2=alpha_g*alpha_g,denom=(ndh*ndh*(a2-1.0)+1.0);
                float D=a2/(3.14159265*denom*denom);
                float g_vis=0.5/max(0.0001,(ndv*sqrt(alpha_g+(1.0-alpha_g)*clNdl*clNdl)+clNdl*sqrt(alpha_g+(1.0-alpha_g)*ndv*ndv)));
                vec3 F=f0_eevee+(vec3(1.0)-f0_eevee)*pow(1.0-vdh,5.0);
                directSpec=F*(D*g_vis*clNdl)*(uLens?uLensSpecularIntensity:uSpecularIntensity);
                if(uEeveeClearcoat>0.001){
                    float cc_alpha=max(0.001,uEeveeClearcoatRoughness*uEeveeClearcoatRoughness),cc_a2=cc_alpha*cc_alpha;
                    float cc_D=cc_a2/(3.14159265*pow(max(0.0001,ndh*ndh*(cc_a2-1.0)+1.0),2.0));
                    float cc_vis=0.25/max(0.0001,pow(1.0+ndh,2.0));
                    float cc_F=0.04+(1.0-0.04)*pow(1.0-vdh,5.0);
                    directSpec+=vec3(cc_F*cc_D*cc_vis*clNdl)*uEeveeClearcoat;
                }
                float fd90=0.5+2.0*rough*vdh*vdh;
                float burley=(1.0+(fd90-1.0)*pow(1.0-clNdl,5.0))*(1.0+(fd90-1.0)*pow(1.0-ndv,5.0));
                diffuse=clNdl*burley;
            }else{
                directSpec=vec3(0.0);
                diffuse=0.0;
            }
        }else {
            directSpec=codSpecularLobe(f0,gloss1,uLens?uLensSpecularIntensity:uSpecularIntensity,normal,lightDirection,viewDirection)+codSpecularLobe(f0,gloss2,uLens?0.0:uSpecularIntensity2,normal,lightDirection,viewDirection);
        }

        if(uBrdfModel>0){
            vec3 h=normalize(lightDirection+viewDirection);
            float ndh=max(dot(normal,h),0.0),ndv=max(dot(normal,viewDirection),0.0),vdh=max(dot(viewDirection,h),0.0),clNdl=max(rawNdl,0.0);
            if(uBrdfModel==1){
                diffuse=clNdl; directSpec=vec3(0.0);
            }else if(uBrdfModel==2){
                float fd90=0.5+2.0*roughness*vdh*vdh;
                diffuse=clNdl*(1.0+(fd90-1.0)*pow(1.0-clNdl,5.0))*(1.0+(fd90-1.0)*pow(1.0-ndv,5.0));
                directSpec=vec3(0.0);
            }else if(uBrdfModel==3){
                float s2=roughness*roughness,A=1.0-0.5*(s2/(s2+0.33)),B=0.45*(s2/(s2+0.09));
                float sinI=sqrt(max(0.0,1.0-clNdl*clNdl)),sinV=sqrt(max(0.0,1.0-ndv*ndv));
                vec3 lProj=normalize(lightDirection-normal*clNdl),vProj=normalize(viewDirection-normal*ndv);
                float cosPhi=max(0.0,dot(lProj,vProj));
                diffuse=clNdl*(A+B*cosPhi*(sinI*sinV)/max(0.001,max(clNdl,ndv)));
                directSpec=vec3(0.0);
            }else if(uBrdfModel==4){
                diffuse=clNdl;
                directSpec=f0*pow(ndh,max(2.0,gloss1))*((gloss1+8.0)/25.132)*clNdl*uSpecularIntensity;
)GLSL" R"GLSL(
            }else if(uBrdfModel==5){
                diffuse=clNdl;
                directSpec=awSpecular(f0,roughness,normal,lightDirection,viewDirection)*uSpecularIntensity;
            }else if(uBrdfModel==6){
                diffuse=clNdl;
                float expVal=max(4.0,gloss1);
                vec3 ashF=f0+(vec3(1.0)-f0)*pow(1.0-vdh,5.0);
                directSpec=ashF*(expVal+1.0)/(25.132*max(0.001,vdh*max(clNdl,ndv)))*pow(ndh,expVal)*clNdl*uSpecularIntensity;
            }else if(uBrdfModel==7){
                diffuse=clNdl;
                float alphaW=max(0.02,roughness),tanT=sqrt(max(0.0,1.0-ndh*ndh))/max(0.001,ndh);
                float wExp=-pow(tanT/alphaW,2.0);
                directSpec=f0*(1.0/max(0.001,12.566*alphaW*alphaW*sqrt(max(0.0001,clNdl*ndv))))*exp(wExp)*clNdl*uSpecularIntensity;
            }else if(uBrdfModel==8){
                float alpha_g=roughness*roughness,a2=alpha_g*alpha_g,denom=(ndh*ndh*(a2-1.0)+1.0);
                float D=a2/(3.14159265*denom*denom);
                float g_vis=0.5/max(0.0001,(ndv*sqrt(alpha_g+(1.0-alpha_g)*clNdl*clNdl)+clNdl*sqrt(alpha_g+(1.0-alpha_g)*ndv*ndv)));
                vec3 F=f0+(vec3(1.0)-f0)*pow(1.0-vdh,5.0);
                directSpec=F*(D*g_vis*clNdl)*uSpecularIntensity;
                float fd90=0.5+2.0*roughness*vdh*vdh;
                diffuse=clNdl*(1.0+(fd90-1.0)*pow(1.0-clNdl,5.0))*(1.0+(fd90-1.0)*pow(1.0-ndv,5.0));
            }else if(uBrdfModel==9){
                diffuse=smoothstep(0.15,0.20,clNdl)*0.4+smoothstep(0.50,0.55,clNdl)*0.6;
                directSpec=f0*smoothstep(0.85,0.90,ndh)*uSpecularIntensity;
            }else if(uBrdfModel==10){
                float wrap=clNdl*0.5+0.5; diffuse=wrap*wrap;
                vec3 scatter=vec3(1.0,0.35,0.15)*pow(max(0.0,-dot(viewDirection,lightDirection)),3.0)*max(0.0,1.0-clNdl)*0.35;
                diffuse+=dot(scatter,vec3(0.333));
                directSpec=awSpecular(f0,max(0.2,roughness),normal,lightDirection,viewDirection)*uSpecularIntensity*0.7;
            }
        }

        vec3 diffuseColor=(uShadingModel==3)?rgb*(1.0-(uSpecularGlossiness?0.0:clamp(uEeveeMetallic,0.0,1.0)))*clamp(vec3(1.0)-f0_eevee_local,vec3(0.0),vec3(1.0)):((uGltfPbr||uShadingModel==2)?rgb*(1.0-metalness)*clamp(vec3(1.0)-f0,vec3(0.0),vec3(1.0)):rgb);
        if(uDecalAdditive||uDecalMultiply)directSpec=vec3(0.0);
        // A Source 2 metallic base color is reflected energy, not white paint.
        // Keep the user's chosen direct-light lobe, but suppress metal diffuse.
        if(source2Pbr)diffuseColor=rgb*(1.0-source2Metal);
        rgb=diffuseColor*clamp(lighting,vec3(0.0),vec3(2.5))+directSpec*uSunColor*uSunIntensity*visibility;
    }
    if(source2Pbr&&!uUnlit&&!uEmissive&&!uSunEnabled)rgb*=1.0-source2Metal;
    if(!uUnlit&&!uEmissive&&!uDecalMultiply&&!uDecalAdditive&&uCubemapSpecular&&uHasSpecCubemap){
        vec3 effNormal=normalize(vNormal+(normal-vNormal)*uNormalReflectionInfluence);
        vec3 reflected=reflect(-viewDirection,effNormal);
        float ndv=clamp(dot(effNormal,viewDirection),0.0,1.0);
        float mappedSpec=clamp(dot(specularSample.rgb,vec3(.2126,.7152,.0722)),0.0,1.0);
        float f0=uGltfPbr?dot(gltfF0,vec3(.2126,.7152,.0722)):(uLens?0.10:mix(0.02,0.28,mappedSpec));
        float fresnel=f0+(1.0-f0)*pow(1.0-ndv,5.0);
        float response=(uLens?uLensCubemapIntensity:uSpecularIntensity)*fresnel;
        if(uWeaponSurface&&uHasMetalnessMap)response*=mix(0.18,1.65,source2Metal);
        if(uDecal)response*=0.05;
        if(uShadingModel==2||uGltfPbr){
            float roughness=uLens?0.025:(uGltfPbr?gltfRough:clamp((1.0-((uHasSpecularMap||uHasRoughnessMap||uSpecularGlossiness)?specularSample.a:base.a))*uAwRoughnessScale+uAwRoughnessBias,.04,1.0));
            vec2 ab=vec2(-1.04,1.04)*pow(1.0-ndv,5.0)+vec2(1.0-roughness,roughness*.04);
            response=max(0.0,(f0*ab.x+ab.y)*(uLens?uLensCubemapIntensity:uSpecularIntensity)*(uGltfPbr?1.0:uAwEnvironmentIntensity));
        }else if(uShadingModel==3){
            float roughness=uLens?0.025:clamp(uHasSpecularMap?(1.0-specularSample.a):uEeveeRoughness,0.025,1.0);
            float f0_d=pow((uEeveeIor-1.0)/max(0.001,uEeveeIor+1.0),2.0)*uEeveeSpecular*2.0;
            vec3 f0_eevee=clamp(mix(vec3(clamp(f0_d,0.0,1.0)),rgb,clamp(uEeveeMetallic,0.0,1.0)),vec3(0.0),vec3(1.0));
            vec2 ab=vec2(-1.04,1.04)*pow(1.0-ndv,5.0)+vec2(1.0-roughness,roughness*.04);
            response=max(0.0,(f0_eevee.x*ab.x+ab.y)*(uLens?uLensCubemapIntensity:uSpecularIntensity));
        }
        float specShadowFactor=uSunEnabled?mix(uShadowSpecularMultiplier,1.0,visibility):1.0;
        float effectiveBlur=uWeaponSurface?uWeaponCubemapBlur:uCubemapBlur;if(uSpecularGlossiness&&!uLens)effectiveBlur*=clamp(1.0-specularSample.a,.04,1.0);if(uWeaponSurface&&uHasRoughnessMap)effectiveBlur*=mix(0.15,2.0,source2Rough);
        float weaponCubemapInt=uCubemapSpecularIntensity*(uViewmodelSurface?uCubemapSurfaceMultipliers.x:uCubemapSurfaceMultipliers.y)*(uWeaponSurface?uWeaponCubemapMultiplier:1.0);
        vec3 reflectionWeight=vec3(response);
        if(source2Pbr||uSpecularGlossiness){
            // Do not compress metallic F0 into the legacy 0.02..0.28 dielectric
            // range or discard its color. All weapon/global multipliers remain.
            vec3 f0Metal=clamp(specularSample.rgb,vec3(0.0),vec3(0.99));
            reflectionWeight=(f0Metal+(vec3(1.0)-f0Metal)*pow(1.0-ndv,5.0))*uSpecularIntensity;
            if(uShadingModel==2)reflectionWeight*=uAwEnvironmentIntensity;
        }
        rgb+=sampleSpecCubeDualKawase(reflected,uGltfPbr?gltfRough*effectiveBlur:effectiveBlur)*weaponCubemapInt*reflectionWeight*specShadowFactor;
        if(uGltfPbr&&uGltfTransmission>0.0){
            vec3 refracted=refract(-viewDirection,effNormal,1.0/max(1.001,uGltfIor));
            vec3 through=sampleSpecCubeDualKawase(refracted,gltfRough*effectiveBlur);
            rgb=mix(rgb,through,clamp(uGltfTransmission*(1.0-gltfMetal),0.0,1.0));
        }
    }
    if(uExplicitMaterial)rgb+=uGltfEmissive*(uHasEmissionMap?texture(uSpecularImperfections,vUv).rgb:vec3(1.0));
    else if(!uEmissive)rgb+=uGltfPbr?uGltfEmissive:vec3(0);
    if(uFogEnabled){
        float visibility=exp2(-max(0.0,length(vWorldPosition-uCameraPosition)-uFogStart)/max(1.0,uFogHalfDistance));
        float heightDensity=uFogHeightEnabled?exp2(-max(0.0,vWorldPosition.z-uFogHeight)/max(1.0,uFogHeightFalloff)):1.0;
        float fogAmount=(1.0-visibility)*heightDensity*clamp(uFogOpacity,0.0,1.0);
        rgb=mix(rgb,uFogColor,clamp(fogAmount,0.0,1.0));
    }
    if(uDecalMultiply){
        rgb=mix(vec3(1.0),base.rgb*vertTint,clamp(alpha,0.0,1.0));
        alpha=1.0;
    }else if(uDecalAdditive){
        rgb=base.rgb*vertTint;
    }
    // Legacy render target is display-encoded; explicit textures entered linear.
    if(uExplicitMaterial)rgb=mix(rgb*12.92,1.055*pow(max(rgb,vec3(0)),vec3(1.0/2.4))-0.055,step(vec3(0.0031308),rgb));
    color=vec4(film(rgb),alpha*uWorldOpacity);
}
)GLSL";

const char* kShadowVertex = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=2) in vec2 aUv;
layout(location=5) in vec4 aColor;
layout(location=3) in uvec4 aBones;
layout(location=4) in vec4 aWeights;
uniform mat4 uLightViewProjection;
uniform mat4 uModel;
uniform samplerBuffer uBones;
uniform samplerBuffer uBoneVisibility;
uniform bool uSkinned;
out float vVisible;
out vec2 vShadowUv;
out float vShadowAlpha;
mat4 boneMatrix(uint index){int base=int(index)*4;return mat4(texelFetch(uBones,base),texelFetch(uBones,base+1),texelFetch(uBones,base+2),texelFetch(uBones,base+3));}
void main(){vShadowUv=aUv;vShadowAlpha=aColor.a;mat4 skin=mat4(1.0);if(uSkinned)skin=boneMatrix(aBones.x)*aWeights.x+boneMatrix(aBones.y)*aWeights.y+boneMatrix(aBones.z)*aWeights.z+boneMatrix(aBones.w)*aWeights.w;vVisible=uSkinned?(texelFetch(uBoneVisibility,int(aBones.x)).r*aWeights.x+texelFetch(uBoneVisibility,int(aBones.y)).r*aWeights.y+texelFetch(uBoneVisibility,int(aBones.z)).r*aWeights.z+texelFetch(uBoneVisibility,int(aBones.w)).r*aWeights.w):1.0;gl_Position=uLightViewProjection*uModel*skin*vec4(aPosition,1.0);}
)GLSL";
const char* kShadowFragment = R"GLSL(
#version 330 core
in float vVisible;
in vec2 vShadowUv;
in float vShadowAlpha;
uniform sampler2D uShadowAlbedo;
uniform bool uShadowMask;
uniform bool uShadowVertexAlpha;
uniform bool uShadowHasAlbedo;
uniform float uShadowCutoff;
uniform float uShadowColorAlpha;
void main(){if(vVisible<0.5)discard;if(uShadowMask){float a=(uShadowHasAlbedo?texture(uShadowAlbedo,vShadowUv).a:1.0)*uShadowColorAlpha*(uShadowVertexAlpha?vShadowAlpha:1.0);if(a<uShadowCutoff)discard;}}
)GLSL";

const char* kLineVertex = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec4 aColor;
uniform mat4 uViewProjection;
out vec4 vColor;
void main(){vColor=aColor;gl_Position=uViewProjection*vec4(aPosition,1.0);}
)GLSL";
const char* kLineFragment = R"GLSL(
#version 330 core
in vec4 vColor; out vec4 color; void main(){color=vColor;}
)GLSL";

const char* kBillboardVertex = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;
layout(location=2) in vec4 aColor;
uniform mat4 uViewProjection;
uniform vec2 uForegroundClip;
out vec2 vTexCoord;
out vec4 vColor;
void main(){
    vTexCoord = aTexCoord;
    vColor = aColor;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
    if(uForegroundClip.x>0.0){float n=uForegroundClip.x,f=uForegroundClip.y;gl_Position.z=((f+n)*gl_Position.w-2.0*f*n)/(f-n);}
}
)GLSL";

const char* kBillboardFragment = R"GLSL(
#version 330 core
in vec2 vTexCoord;
in vec4 vColor;
uniform sampler2D uTexture;
uniform int uUseTexture;
uniform int uProceduralMode;
uniform float uFeathering;
uniform float uTrailEdgeDarkening;
uniform float uTrailEdgePower;
uniform vec3 uTrailEdgeTint;
out vec4 fragColor;
void main(){
    vec4 tex = vec4(1.0);
    if (uUseTexture != 0) {
        tex = texture(uTexture, vTexCoord);
        if (uTrailEdgeDarkening > 0.001) {
            float d = abs(vTexCoord.y - 0.5) * 2.0;
            float edgeTerm = pow(clamp(d, 0.0, 1.0), max(0.01, uTrailEdgePower));
            vec3 edgeGradient = mix(vec3(1.0), uTrailEdgeTint * (1.0 - uTrailEdgeDarkening), edgeTerm);
            tex.rgb *= edgeGradient;
        }
    } else if (uProceduralMode == 1) {
        float d = length(vTexCoord - vec2(0.5)) * 2.0;
        float alpha = clamp(1.0 - d * d, 0.0, 1.0);
        alpha = smoothstep(0.0, 1.0, alpha);
        float edgeTerm = pow(clamp(d, 0.0, 1.0), max(0.01, uTrailEdgePower));
        vec3 edgeGradient = mix(vec3(1.0), uTrailEdgeTint * (1.0 - uTrailEdgeDarkening), edgeTerm);
        tex = vec4(edgeGradient, alpha);
    } else if (uProceduralMode == 2) {
        vec2 p = vTexCoord - vec2(0.5);
        float r = length(p);
        float angle = atan(p.y, p.x);
        float rays = max(0.0, cos(angle * 4.0)) * 0.35 + max(0.0, cos(angle * 8.0)) * 0.15;
        float core = exp(-r * 12.0);
        float glow = exp(-r * 4.0) * (0.6 + rays);
        float alpha = clamp(core + glow, 0.0, 1.0);
        tex = vec4(1.0, 0.9, 0.6, 1.0) * alpha;
    } else if (uProceduralMode == 3) {
        float d = abs(vTexCoord.y - 0.5) * 2.0;
        float alpha = 1.0;
        if (uFeathering > 0.001) {
            float edge = clamp(1.0 - uFeathering, 0.0, 0.999);
            alpha = 1.0 - smoothstep(edge, 1.0, d);
        }
        float edgeTerm = pow(clamp(d, 0.0, 1.0), max(0.01, uTrailEdgePower));
        vec3 edgeGradient = mix(vec3(1.0), uTrailEdgeTint * (1.0 - uTrailEdgeDarkening), edgeTerm);
        tex = vec4(edgeGradient, alpha);
    } else if (uProceduralMode == 4) {
        vec2 p = vTexCoord - vec2(0.5);
        float r = length(p);
        float alpha = clamp(1.0 - r * 2.0, 0.0, 1.0);
        if (uFeathering > 0.001) {
            float edge = clamp(1.0 - uFeathering, 0.0, 0.999);
            alpha = smoothstep(edge, 1.0, alpha);
        }
        float noise = fract(sin(dot(vTexCoord, vec2(12.9898, 78.233))) * 43758.5453);
        alpha *= (0.70 + 0.30 * noise);
        tex = vec4(1.0, 1.0, 1.0, alpha);
    }
    vec4 finalCol = vColor * tex;
    if (finalCol.a < 0.003) discard;
    fragColor = finalCol;
}
)GLSL";

const char* kEnvironmentVertex = R"GLSL(
#version 330 core
out vec2 vScreen;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);vScreen=p*2.0-1.0;gl_Position=vec4(vScreen,1.0,1.0);}
)GLSL";
const char* kEnvironmentFragment = R"GLSL(
#version 330 core
in vec2 vScreen;out vec4 color;
uniform int uMode;uniform int uDebugView;uniform sampler2D uPanorama;uniform sampler2D uFront;uniform sampler2D uBack;uniform sampler2D uLeft;uniform sampler2D uRight;uniform sampler2D uUp;uniform sampler2D uDown;
uniform int uFaceSource0;uniform int uFaceSource1;uniform int uFaceSource2;uniform int uFaceSource3;uniform int uFaceSource4;uniform int uFaceSource5;
uniform int uFaceRotation0;uniform int uFaceRotation1;uniform int uFaceRotation2;uniform int uFaceRotation3;uniform int uFaceRotation4;uniform int uFaceRotation5;
uniform vec3 uForward;uniform vec3 uRightAxis;uniform vec3 uUpAxis;uniform float uTanHalfFov;uniform float uAspect;uniform float uRotation;uniform float uIntensity;uniform float uExposure;uniform bool uFlipVertical;uniform float uSkyContrast;uniform float uSkyHighlightBoost;uniform float uSkyHighlightThreshold;uniform float uSkyToneMap;
uniform bool uFilmEnabled;uniform float uFilmBrightness;uniform float uFilmContrast;uniform float uFilmDesaturation;uniform vec3 uFilmDarkTint;uniform vec3 uFilmMidTint;uniform vec3 uFilmLightTint;uniform bool uFilmMidTintEnabled;uniform bool uFilmInvert;uniform bool uBloomEnabled;uniform float uBloomThreshold;uniform float uBloomIntensity;uniform bool uVignetteEnabled;uniform float uVignetteIntensity;uniform float uVignetteRadius;uniform float uVignetteSoftness;uniform vec2 uViewportSize;uniform vec3 uFogColor;uniform float uFogSkyAmount;
)GLSL" CADENCE_DAY_NIGHT_SKY_GLSL R"GLSL(
int faceSource(int face){if(face==0)return uFaceSource0;if(face==1)return uFaceSource1;if(face==2)return uFaceSource2;if(face==3)return uFaceSource3;if(face==4)return uFaceSource4;return uFaceSource5;}
int faceRotation(int face){if(face==0)return uFaceRotation0;if(face==1)return uFaceRotation1;if(face==2)return uFaceRotation2;if(face==3)return uFaceRotation3;if(face==4)return uFaceRotation4;return uFaceRotation5;}
vec2 rotateFace(vec2 uv,int turns){int q=((turns%4)+4)%4;if(q==1)return vec2(1.0-uv.y,uv.x);if(q==2)return vec2(1.0-uv.x,1.0-uv.y);if(q==3)return vec2(uv.y,1.0-uv.x);return uv;}
vec3 rawFace(int source,vec2 uv){if(source==0)return texture(uFront,uv).rgb;if(source==1)return texture(uBack,uv).rgb;if(source==2)return texture(uLeft,uv).rgb;if(source==3)return texture(uRight,uv).rgb;if(source==4)return texture(uUp,uv).rgb;return texture(uDown,uv).rgb;}
vec3 logicalFace(int face,vec2 uv){return rawFace(faceSource(face),rotateFace(uv,faceRotation(face)));}
vec3 faceSample(vec3 worldDirection){
    // T6 stores a Direct3D cubemap. Its world basis is (X,-Z,Y) relative to
    // the stage, and raw D3D scanlines need a V flip when uploaded as GL 2D faces.
    vec3 d=vec3(worldDirection.x,-worldDirection.y,worldDirection.z),a=abs(d);vec2 q;vec3 sampleColor;
    if(a.x>=a.y&&a.x>=a.z){if(d.x>=0.0){q=vec2(-d.z,-d.y)/a.x;sampleColor=logicalFace(0,vec2(q.x,-q.y)*.5+.5);}else{q=vec2(d.z,-d.y)/a.x;sampleColor=logicalFace(1,vec2(q.x,-q.y)*.5+.5);}}
    else if(a.y>=a.z){if(d.y>=0.0){q=vec2(d.x,d.z)/a.y;sampleColor=logicalFace(2,vec2(q.x,-q.y)*.5+.5);}else{q=vec2(d.x,-d.z)/a.y;sampleColor=logicalFace(3,vec2(q.x,-q.y)*.5+.5);}}
    else {if(d.z>=0.0){q=vec2(d.x,-d.y)/a.z;sampleColor=logicalFace(4,vec2(q.x,-q.y)*.5+.5);}else{q=vec2(-d.x,-d.y)/a.z;sampleColor=logicalFace(5,vec2(q.x,-q.y)*.5+.5);}}
    return sampleColor;
}
vec3 film(vec3 rgb){if(!uFilmEnabled)return rgb;float l=dot(rgb,vec3(.2126,.7152,.0722));rgb=mix(rgb,vec3(l),clamp(uFilmDesaturation,-1.0,1.0));rgb=(rgb-.5)*uFilmContrast+.5+uFilmBrightness;vec3 tint=mix(uFilmDarkTint,uFilmLightTint,clamp(l,0.0,1.0));if(uFilmMidTintEnabled)tint=mix(tint,uFilmMidTint,clamp(1.0-abs(l*2.0-1.0),0.0,1.0));rgb*=tint;if(uBloomEnabled){vec3 bright=max(rgb-vec3(uBloomThreshold),vec3(0));rgb+=bright*bright/(bright+vec3(.25))*uBloomIntensity;}if(uVignetteEnabled){vec2 uv=gl_FragCoord.xy/max(uViewportSize,vec2(1));float d=length((uv-.5)*vec2(uViewportSize.x/uViewportSize.y,1));float v=smoothstep(uVignetteRadius,max(uVignetteRadius+.001,uVignetteRadius+uVignetteSoftness),d);rgb*=1.0-v*clamp(uVignetteIntensity,0.0,1.0);}if(uFilmInvert)rgb=vec3(1)-rgb;return max(rgb,vec3(0));}
void main(){if(uDebugView==7){color=vec4(1);return;}if(uDebugView==8){color=vec4(0,1,0,1);return;}vec3 d=normalize(uForward+uRightAxis*(vScreen.x*uAspect*uTanHalfFov)+uUpAxis*(vScreen.y*uTanHalfFov));if(uDayNightSky){color=vec4(dayNightSky(d),1.0);return;}float c=cos(uRotation),s=sin(uRotation);d=vec3(d.x*c-d.y*s,d.x*s+d.y*c,d.z);if(uFlipVertical)d.z=-d.z;vec3 rgb;if(uMode==1){vec2 uv=vec2(atan(d.y,d.x)/6.2831853+.5,asin(clamp(d.z,-1.0,1.0))/3.14159265+.5);rgb=texture(uPanorama,uv).rgb;}else rgb=faceSample(d);rgb*=uIntensity*exp2(uExposure);rgb=max(vec3(0),.5+(rgb-.5)*uSkyContrast);float l=max(rgb.r,max(rgb.g,rgb.b)),highlight=smoothstep(uSkyHighlightThreshold,max(uSkyHighlightThreshold+.001,1.0),l);rgb*=mix(1.0,uSkyHighlightBoost,highlight);rgb=mix(rgb,rgb/(rgb+vec3(1)),clamp(uSkyToneMap,0.0,1.0));rgb=mix(rgb,uFogColor,clamp(uFogSkyAmount,0.0,1.0));color=vec4(rgb,1.0);}
)GLSL";

const char* kPostFragment=R"GLSL(
#version 330 core
#define main cadencePostMain
in vec2 vScreen;out vec4 color;uniform sampler2D uColor;uniform sampler2D uDepth;uniform sampler2D uBloomTexture;uniform sampler2D uLut;uniform vec2 uTexel;uniform bool uBloom;uniform float uBloomIntensity;uniform bool uLutEnabled;uniform float uLutSize;uniform float uLutIntensity;uniform float uDistortion;uniform bool uFilmEnabled;uniform float uFilmBrightness;uniform float uFilmContrast;uniform float uFilmDesaturation;uniform vec3 uFilmDarkTint;uniform vec3 uFilmMidTint;uniform vec3 uFilmLightTint;uniform bool uFilmMidTintEnabled;uniform bool uFilmInvert;uniform bool uVignetteEnabled;uniform float uVignetteIntensity;uniform float uVignetteRadius;uniform float uVignetteSoftness;uniform bool uAutoBlack;uniform float uAutoBlackIntensity;uniform bool uAutoWhite;uniform float uAutoWhiteIntensity;uniform float uCameraNear;uniform float uCameraFar;uniform int uDebugView;uniform float uDebugDepthNear;uniform float uDebugDepthFar;uniform bool uDebugDepthInvert;
uniform sampler2D uHbao;uniform bool uHbaoEnabled,uHbaoPreview;uniform float uHbaoIntensity,uHbaoPower;
uniform bool uSeparateForeground;uniform vec2 uViewmodelDepthRange;
uniform sampler2D uAoIsolated,uDofFar,uDofNear;uniform bool uAoIsolationEnabled,uDofEnabled,uDofPreview;
)GLSL" CADENCE_AO_FOG_GLSL R"GLSL(
uniform int uTonemapping;
uniform int uBloomBlendMode;
uniform int uPostPassOrder[7];
uniform bool uTonemapRec709Match;
uniform float uTonemapRec709Strength;
// Small coverage-weighted reconstruction filter reduces fixed gather patterns;
// retain center coverage so focused/viewmodel pixels are not painted over.
vec4 filteredDof(sampler2D image,vec2 uv){vec4 center=texture(image,uv);if(center.a<.001)return center;vec2 texel=1.0/vec2(textureSize(image,0));vec3 sum=center.rgb*center.a*4;float weight=center.a*4;for(int i=0;i<4;++i){vec2 axis=i==0?vec2(1,0):i==1?vec2(-1,0):i==2?vec2(0,1):vec2(0,-1);vec4 tap=texture(image,uv+axis*texel*.75);float w=tap.a*step(center.a*.25,tap.a);sum+=tap.rgb*w;weight+=w;}return vec4(sum/max(weight,.00001),center.a);}
vec3 lut(vec3 c){float n=uLutSize,b=clamp(c.b,0,1)*(n-1),s0=floor(b),s1=min(n-1,s0+1);vec2 uv0=vec2((s0*n+clamp(c.r,0,1)*(n-1)+.5)/(n*n),(clamp(c.g,0,1)*(n-1)+.5)/n),uv1=vec2((s1*n+clamp(c.r,0,1)*(n-1)+.5)/(n*n),(clamp(c.g,0,1)*(n-1)+.5)/n);return mix(texture(uLut,uv0).rgb,texture(uLut,uv1).rgb,fract(b));}
vec4 catmull(sampler2D tex,vec2 uv){vec2 p=uv/uTexel-.5,b=floor(p),f=fract(p);float fx2=f.x*f.x,fx3=fx2*f.x,fy2=f.y*f.y,fy3=fy2*f.y;vec4 wx=vec4(-.5*f.x+fx2-.5*fx3,1.0-2.5*fx2+1.5*fx3,.5*f.x+2.0*fx2-1.5*fx3,-.5*fx2+.5*fx3),wy=vec4(-.5*f.y+fy2-.5*fy3,1.0-2.5*fx2+1.5*fx3,.5*f.y+2.0*fx2-1.5*fx3,-.5*fx2+.5*fx3);vec4 result=vec4(0);for(int y=0;y<4;++y)for(int x=0;x<4;++x)result+=texture(tex,(b+vec2(x-1,y-1)+.5)*uTexel)*wx[x]*wy[y];return result;}
float linearDepth(float depth){
    if(uSeparateForeground&&depth<.0400001){depth=clamp(depth/.04,0.0,1.0);return uViewmodelDepthRange.x*uViewmodelDepthRange.y/max(.000001,uViewmodelDepthRange.y-depth*(uViewmodelDepthRange.y-uViewmodelDepthRange.x));}
    return uCameraNear*uCameraFar/max(0.0001,uCameraFar-depth*(uCameraFar-uCameraNear));
}
vec3 aces13(vec3 x){
    const mat3 m1=mat3(0.59719,0.07600,0.02840,0.35458,0.90834,0.13383,0.04823,0.01566,0.83777);
    const mat3 m2=mat3(1.60475,-0.10208,-0.00327,-0.53108,1.10813,-0.07276,-0.07367,-0.00605,1.07602);
    vec3 v=m1*x;
    vec3 a=v*(v+0.0245786)-0.000090537;
    vec3 b=v*(0.983729*v+0.4329510)+0.238081;
    return clamp(m2*(a/b),0.0,1.0);
}
vec3 aces20(vec3 col){
    const mat3 srgbToAp1=mat3(0.613097,0.070194,0.020616,0.339523,0.916354,0.109570,0.047379,0.013452,0.869814);
    const mat3 ap1ToSrgb=mat3(1.704859,-0.130078,-0.023964,-0.621716,1.140803,-0.128975,-0.083299,-0.010530,1.152940);
    vec3 ap1=max(vec3(0.0),srgbToAp1*col);
    float lum=dot(ap1,vec3(0.2722287,0.6740818,0.0536895));
    float cLum=lum/(1.0+lum);
    cLum=pow(cLum,0.95);
    vec3 tc=(lum>1e-6)?ap1*(cLum/lum):vec3(0.0);
    float maxC=max(tc.r,max(tc.g,tc.b));
    if(maxC>0.8){
        float desat=clamp((maxC-0.8)/0.8,0.0,1.0);
        tc=mix(tc,vec3(cLum),desat*0.4);
    }
    vec3 res=ap1ToSrgb*tc;
    return clamp(res,0.0,1.0);
}
vec3 filmicTone(vec3 x){
    vec3 logCol=clamp((log2(max(x,vec3(1e-5)))+10.0)/16.5,0.0,1.0);
    vec3 s=logCol*logCol*(3.0-2.0*logCol);
    float l=dot(s,vec3(0.2126,0.7152,0.0722));
    float maxVal=max(s.r,max(s.g,s.b));
    if(maxVal>0.7){
        float desat=clamp((maxVal-0.7)/0.3,0.0,1.0);
        s=mix(s,vec3(l),desat*0.5);
    }
    return clamp(s,0.0,1.0);
}
vec3 agxTone(vec3 col){
    const mat3 agxInset=mat3(
        0.842479062253094,0.0423282422610123,0.0423756549057051,
        0.078433599699346,0.878468636497787,0.078433600000000,
        0.079223726457788,0.079166127460543,0.879190745094295
    );
    const mat3 agxOutset=mat3(
        1.196879024257647,-0.052896851757556,-0.052971635584449,
        -0.098020881140136,1.151903129904172,-0.098043450117124,
        -0.099029744079720,-0.098961176844843,1.151015085701573
    );
    vec3 val=max(vec3(1e-6),agxInset*col);
    vec3 logVal=clamp((log2(val)+10.0)/16.5,0.0,1.0);
    vec3 x2=logVal*logVal;
    vec3 x4=x2*x2;
    vec3 sig=15.5*x4*logVal-40.14*x4+31.96*x2*logVal-6.868*x2+0.429*logVal+0.119;
    return clamp(agxOutset*sig,0.0,1.0);
}
vec3 applyFilm(vec3 rgb){
    if(uFilmEnabled){
        float l=dot(rgb,vec3(.2126,.7152,.0722));
        rgb=mix(rgb,vec3(l),clamp(uFilmDesaturation,-1.0,1.0));
        rgb=(rgb-.5)*uFilmContrast+.5+uFilmBrightness;
        vec3 tint=mix(uFilmDarkTint,uFilmLightTint,clamp(l,0.0,1.0));
        if(uFilmMidTintEnabled)tint=mix(tint,uFilmMidTint,clamp(1.0-abs(l*2.0-1.0),0.0,1.0));
        rgb*=tint;
        if(uFilmInvert)rgb=vec3(1)-rgb;
    }
    return max(rgb,vec3(0));
}
vec3 applyVignette(vec3 rgb,vec2 uv){
    if(uVignetteEnabled){
        float d=length((uv-.5)*vec2(uTexel.y/uTexel.x,1));
        float v=smoothstep(uVignetteRadius,max(uVignetteRadius+.001,uVignetteRadius+uVignetteSoftness),d);
        rgb*=1.0-v*clamp(uVignetteIntensity,0.0,1.0);
    }
    return max(rgb,vec3(0));
}
vec3 applyTonemap(vec3 rgb){
    if(uTonemapping==1)return aces13(rgb);
    if(uTonemapping==2)return aces20(rgb);
    if(uTonemapping==3)return filmicTone(rgb);
    if(uTonemapping==4)return agxTone(rgb);
    return rgb;
}
vec3 applyRec709Match(vec3 rgb,vec3 linearIn){
    if(!uTonemapRec709Match||uTonemapRec709Strength<=0.001)return rgb;
    float linLum=max(1e-5,dot(linearIn,vec3(0.2126,0.7152,0.0722)));
    float targetY=(linLum<0.018)?(4.500*linLum):(1.099*pow(linLum,0.45)-0.099);
    targetY=clamp(targetY,0.0,1.0);
    float curLum=max(1e-5,dot(rgb,vec3(0.2126,0.7152,0.0722)));
    float ratio=targetY/curLum;
    float weight=smoothstep(1.0,0.30,curLum);
    vec3 corrected=clamp(rgb*mix(1.0,ratio,weight),0.0,1.0);
    return mix(rgb,corrected,clamp(uTonemapRec709Strength,0.0,1.0));
}
vec3 applyLevels(vec3 rgb){
    if(uAutoBlack||uAutoWhite){
        float black=uAutoBlack?clamp(uAutoBlackIntensity,0.0,.999):0.0;
        float white=uAutoWhite?clamp(uAutoWhiteIntensity,black+.001,1.0):1.0;
        rgb=(rgb-vec3(black))/max(.001,white-black);
    }
    return max(rgb,vec3(0));
}
void main(){
    vec2 p=vScreen;
    if(uDistortion>0.0){
        float s=1.0/(1.0+2.0*uDistortion);
        for(int i=0;i<3;++i)s-=(2.0*uDistortion*s*s*s+s-1.0)/(6.0*uDistortion*s*s+1.0);
        p*=s;
    }
    vec2 uv=.5+.5*p*(1.0+uDistortion*dot(p,p));
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1)))){color=vec4(0,0,0,1);return;}
    vec4 base=texture(uColor,uv);
    float visibility=uHbaoEnabled?pow(clamp(1.0-(1.0-texture(uHbao,uv).r)*uHbaoIntensity,0.0,1.0),uHbaoPower):1.0;
    if(uHbaoEnabled&&uHbaoPreview){color=vec4(vec3(visibility),base.a);return;}
    vec3 rgb=base.rgb*visibility;
    rgb+=aoFogRestore(uv,visibility);
    if(uAoIsolationEnabled)rgb+=texture(uAoIsolated,uv).rgb*(1-visibility);
    if(uDofEnabled){vec4 farBlur=filteredDof(uDofFar,uv),nearBlur=filteredDof(uDofNear,uv);if(uDofPreview){color=vec4(nearBlur.a,0,farBlur.a,base.a);return;}rgb=mix(rgb,farBlur.rgb,farBlur.a);rgb=mix(rgb,nearBlur.rgb,nearBlur.a);}
    vec3 linearSceneColor=rgb;

    for(int passStep=0;passStep<7;++passStep){
        int pass=uPostPassOrder[passStep];
        if(pass==0){
            if(uBloom){
                vec3 b=texture(uBloomTexture,uv).rgb*.25*uBloomIntensity;
                if(uBloomBlendMode==0){
                    rgb+=b;
                }else if(uBloomBlendMode==1){
                    rgb=1.0-(1.0-rgb)*(1.0-clamp(b,0.0,1.0));
                }else if(uBloomBlendMode==2){
                    vec3 bCl=clamp(b,0.0,1.0);
                    rgb=mix(2.0*rgb*bCl+rgb*rgb*(1.0-2.0*bCl),sqrt(max(rgb,vec3(0.0)))*(2.0*bCl-1.0)+2.0*rgb*(1.0-bCl),step(0.5,bCl));
                }else if(uBloomBlendMode==3){
                    rgb=max(rgb,b);
                }else if(uBloomBlendMode==4){
                    rgb=rgb/max(vec3(0.01),vec3(1.0)-clamp(b,0.0,0.99));
                }
            }
        }else if(pass==1){
            rgb=applyFilm(rgb);
        }else if(pass==2){
            rgb=applyVignette(rgb,uv);
        }else if(pass==3){
            // Distortion is already incorporated in screen UV coordinates
        }else if(pass==4){
            rgb=applyTonemap(rgb);
            rgb=applyRec709Match(rgb,linearSceneColor);
        }else if(pass==5){
            rgb=applyLevels(rgb);
        }else if(pass==6){
            if(uLutEnabled)rgb=mix(rgb,lut(rgb),clamp(uLutIntensity,0.0,1.0));
        }
    }
    color=vec4(rgb,base.a);
}
#undef main
void main(){
    if(uDebugView==7){
        vec2 uv=vScreen*.5+.5;
        float raw=texture(uDepth,uv).r;
        if(raw>=1.0){color=vec4(0,0,0,1);return;}
        float d=linearDepth(raw),v=1.0-clamp((d-uDebugDepthNear)/max(.001,uDebugDepthFar-uDebugDepthNear),0.0,1.0);
        if(uDebugDepthInvert)v=1.0-v;
        color=vec4(vec3(v),1);
        return;
    }
    cadencePostMain();
}
)GLSL";

const char* kKawaseFragment=R"GLSL(
#version 330 core
in vec2 vScreen;out vec4 color;uniform sampler2D uSource;uniform vec2 uTexel;uniform float uOffset;uniform bool uThreshold;uniform float uThresholdValue;uniform float uThresholdSoftness;uniform float uSaturationBias;uniform float uAspect;uniform float uRotation;
void main(){vec2 uv=vScreen*.5+.5;float c=cos(uRotation),s=sin(uRotation);vec2 base=vec2(uTexel.x*max(.01,uAspect),uTexel.y/max(.01,uAspect))*uOffset;vec2 o=vec2(base.x*c-base.y*s,base.x*s+base.y*c);vec3 rgb=(texture(uSource,uv-o).rgb+texture(uSource,uv+vec2(o.x,-o.y)).rgb+texture(uSource,uv+vec2(-o.x,o.y)).rgb+texture(uSource,uv+o).rgb)*.25;if(uThreshold){float l=max(rgb.r,max(rgb.g,rgb.b)),weight=smoothstep(uThresholdValue-uThresholdSoftness,uThresholdValue+uThresholdSoftness,l);rgb*=weight;float gray=dot(rgb,vec3(.2126,.7152,.0722));rgb=mix(vec3(gray),rgb,max(0.0,1.0+uSaturationBias));}color=vec4(rgb,1);}
)GLSL";

void pushLine(std::vector<float>& data, scene::Vec3 a, scene::Vec3 b, scene::Vec4 color) {
    for(auto p:{a,b}) { data.insert(data.end(),{p.x,p.y,p.z,color.x,color.y,color.z,color.w}); }
}

void pushTriangle(std::vector<float>& data,scene::Vec3 a,scene::Vec3 b,scene::Vec3 c,scene::Vec4 color){for(const auto p:{a,b,c})data.insert(data.end(),{p.x,p.y,p.z,color.x,color.y,color.z,color.w});}

void pushSolidBox(std::vector<float>& data,const scene::course::Box& box,scene::Vec4 color){const scene::Vec3 p[8]={{box.minX,box.minY,0},{box.maxX,box.minY,0},{box.maxX,box.maxY,0},{box.minX,box.maxY,0},{box.minX,box.minY,box.height},{box.maxX,box.minY,box.height},{box.maxX,box.maxY,box.height},{box.minX,box.maxY,box.height}};const int faces[5][4]={{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}};for(const auto& face:faces){pushTriangle(data,p[face[0]],p[face[1]],p[face[2]],color);pushTriangle(data,p[face[0]],p[face[2]],p[face[3]],color);}}

void pushBox(std::vector<float>& data,const scene::course::Box& box,scene::Vec4 color){
    const scene::Vec3 p[8]={{box.minX,box.minY,0},{box.maxX,box.minY,0},{box.maxX,box.maxY,0},{box.minX,box.maxY,0},
                            {box.minX,box.minY,box.height},{box.maxX,box.minY,box.height},{box.maxX,box.maxY,box.height},{box.minX,box.maxY,box.height}};
    constexpr int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    for(const auto& edge:edges)pushLine(data,p[edge[0]],p[edge[1]],color);
}

using texture::downscaleRgba;
using texture::TgaImage;
using texture::loadTga;

} // namespace

StageRenderer::~StageRenderer(){shutdown();}

int StageRenderer::uniformLocation(unsigned program,std::string_view name){
    auto& locations=uniformLocations_[program];
    if(const auto found=locations.find(name);found!=locations.end())return found->second;
    const std::string key(name);
    const int location=glapi::GetUniformLocation(program,key.c_str());
    locations.emplace(key,location); // Missing/optimized-out uniforms (-1) are cached too.
    return location;
}

unsigned StageRenderer::compileProgram(const char* vertex,const char* fragment,std::string& error) {
    auto compile=[&](GLenum type,const char* source)->unsigned {
        const auto shader=glapi::CreateShader(type); glapi::ShaderSource(shader,1,&source,nullptr); glapi::CompileShader(shader);
        GLint ok{}; glapi::GetShaderiv(shader,glapi::CompileStatus,&ok);
        if(!ok){char log[2048]{};glapi::GetShaderInfoLog(shader,sizeof(log),nullptr,log);error=log;glapi::DeleteShader(shader);return 0;}
        return shader;
    };
    const auto vs=compile(glapi::VertexShader,vertex); if(!vs)return 0;
    const auto fs=compile(glapi::FragmentShader,fragment); if(!fs){glapi::DeleteShader(vs);return 0;}
    const auto program=glapi::CreateProgram();glapi::AttachShader(program,vs);glapi::AttachShader(program,fs);glapi::LinkProgram(program);
    glapi::DeleteShader(vs);glapi::DeleteShader(fs);
    GLint ok{};glapi::GetProgramiv(program,glapi::LinkStatus,&ok);
    if(!ok){char log[2048]{};glapi::GetProgramInfoLog(program,sizeof(log),nullptr,log);error=log;glapi::DeleteProgram(program);return 0;}
    return program;
}

bool StageRenderer::initialize(std::string& error) {
    if(initialized_)return true;
    uniformLocations_.clear();
    if(!glapi::load()){error="OpenGL 3.3 function loading failed";return false;}
    meshProgram_=compileProgram(kMeshVertex,kMeshFragment,error);if(!meshProgram_)return false;
    lineProgram_=compileProgram(kLineVertex,kLineFragment,error);if(!lineProgram_){glapi::DeleteProgram(meshProgram_);meshProgram_=0;return false;}
    billboardProgram_=compileProgram(kBillboardVertex,kBillboardFragment,error);if(!billboardProgram_){glapi::DeleteProgram(lineProgram_);glapi::DeleteProgram(meshProgram_);lineProgram_=meshProgram_=0;return false;}
    shadowProgram_=compileProgram(kShadowVertex,kShadowFragment,error);if(!shadowProgram_){glapi::DeleteProgram(billboardProgram_);glapi::DeleteProgram(lineProgram_);glapi::DeleteProgram(meshProgram_);billboardProgram_=lineProgram_=meshProgram_=0;return false;}
    environmentProgram_=compileProgram(kEnvironmentVertex,kEnvironmentFragment,error);if(!environmentProgram_){glapi::DeleteProgram(shadowProgram_);glapi::DeleteProgram(billboardProgram_);glapi::DeleteProgram(lineProgram_);glapi::DeleteProgram(meshProgram_);shadowProgram_=billboardProgram_=lineProgram_=meshProgram_=0;return false;}
    postProgram_=compileProgram(kEnvironmentVertex,kPostFragment,error);if(!postProgram_)return false;
    hbaoProgram_=compileProgram(kEnvironmentVertex,kHbaoFragment,error);if(!hbaoProgram_)return false;
    dofProgram_=compileProgram(kEnvironmentVertex,kDofFragment,error);if(!dofProgram_)return false;
    kawaseProgram_=compileProgram(kEnvironmentVertex,kKawaseFragment,error);if(!kawaseProgram_)return false;
    glapi::GenFramebuffers(1,&shadowFramebuffer_);if(!resizeShadowTarget(requestedShadowResolution_)){error="Shadow framebuffer creation failed";return false;}glapi::GenFramebuffers(1,&viewmodelShadowFramebuffer_);if(!resizeViewmodelShadowTarget(requestedViewmodelShadowResolution_)){error="Viewmodel shadow framebuffer creation failed";return false;}glapi::GenFramebuffers(1,&farShadowFramebuffer_);if(!resizeFarShadowTarget(requestedFarShadowResolution_)){error="Far shadow framebuffer creation failed";return false;}
    glapi::GenVertexArrays(1,&lineVao_);glapi::GenBuffers(1,&lineBuffer_);
    glapi::BindVertexArray(lineVao_);glapi::BindBuffer(glapi::ArrayBuffer,lineBuffer_);
    glapi::EnableVertexAttribArray(0);glapi::VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,7*sizeof(float),nullptr);
    glapi::EnableVertexAttribArray(1);glapi::VertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,7*sizeof(float),reinterpret_cast<void*>(3*sizeof(float)));
    glapi::GenVertexArrays(1,&billboardVao_);glapi::GenBuffers(1,&billboardBuffer_);
    glapi::BindVertexArray(billboardVao_);glapi::BindBuffer(glapi::ArrayBuffer,billboardBuffer_);
    glapi::EnableVertexAttribArray(0);glapi::VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,9*sizeof(float),nullptr);
    glapi::EnableVertexAttribArray(1);glapi::VertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,9*sizeof(float),reinterpret_cast<void*>(3*sizeof(float)));
    glapi::EnableVertexAttribArray(2);glapi::VertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,9*sizeof(float),reinterpret_cast<void*>(5*sizeof(float)));
    glapi::GenBuffers(1,&boneBuffer_);glGenTextures(1,&boneTexture_);
    glapi::GenBuffers(1,&visibilityBuffer_);glGenTextures(1,&visibilityTexture_);
    meshUniforms_.model=uniformLocation(meshProgram_,"uModel");
    meshUniforms_.color=uniformLocation(meshProgram_,"uColor");
    meshUniforms_.ignoreAlpha=uniformLocation(meshProgram_,"uIgnoreAlpha");
    meshUniforms_.viewmodelSurface=uniformLocation(meshProgram_,"uViewmodelSurface");
    meshUniforms_.gltfPbr=uniformLocation(meshProgram_,"uGltfPbr");
    meshUniforms_.specularGlossiness=uniformLocation(meshProgram_,"uSpecularGlossiness");
    meshUniforms_.gltfMetallic=uniformLocation(meshProgram_,"uGltfMetallic");
    meshUniforms_.gltfRoughness=uniformLocation(meshProgram_,"uGltfRoughness");
    meshUniforms_.gltfTransmission=uniformLocation(meshProgram_,"uGltfTransmission");
    meshUniforms_.gltfIor=uniformLocation(meshProgram_,"uGltfIor");
    meshUniforms_.gltfEmissive=uniformLocation(meshProgram_,"uGltfEmissive");
    meshUniforms_.skinned=uniformLocation(meshProgram_,"uSkinned");
    meshUniforms_.hasAlbedo=uniformLocation(meshProgram_,"uHasAlbedo");
    meshUniforms_.camoLumaMask=uniformLocation(meshProgram_,"uCamoLumaMask");
    meshUniforms_.camoLumaLow=uniformLocation(meshProgram_,"uCamoLumaLow");
    meshUniforms_.camoLumaHigh=uniformLocation(meshProgram_,"uCamoLumaHigh");
    meshUniforms_.camoLumaGamma=uniformLocation(meshProgram_,"uCamoLumaGamma");
    meshUniforms_.camoLumaContrast=uniformLocation(meshProgram_,"uCamoLumaContrast");
    meshUniforms_.camoInvert=uniformLocation(meshProgram_,"uCamoInvert");
    meshUniforms_.hasNormalMap=uniformLocation(meshProgram_,"uHasNormalMap");
    meshUniforms_.normalIntensity=uniformLocation(meshProgram_,"uNormalIntensity");
    meshUniforms_.hasSpecularMap=uniformLocation(meshProgram_,"uHasSpecularMap");
    meshUniforms_.hasMetalnessMap=uniformLocation(meshProgram_,"uHasMetalnessMap");
    meshUniforms_.source2Material=uniformLocation(meshProgram_,"uSource2Material");
    meshUniforms_.hasRoughnessMap=uniformLocation(meshProgram_,"uHasRoughnessMap");
    meshUniforms_.lens=uniformLocation(meshProgram_,"uLens");
    meshUniforms_.eyeOverlay=uniformLocation(meshProgram_,"uEyeOverlay");
    meshUniforms_.explicitMaterial=uniformLocation(meshProgram_,"uExplicitMaterial");
    meshUniforms_.unlit=uniformLocation(meshProgram_,"uUnlit");
    meshUniforms_.vertexColor=uniformLocation(meshProgram_,"uVertexColor");
    meshUniforms_.alphaCutoff=uniformLocation(meshProgram_,"uAlphaCutoff");
    meshUniforms_.hasEmissionMap=uniformLocation(meshProgram_,"uHasEmissionMap");
    meshUniforms_.emissive=uniformLocation(meshProgram_,"uEmissive");
    meshUniforms_.forceAlpha=uniformLocation(meshProgram_,"uForceAlpha");
    meshUniforms_.decal=uniformLocation(meshProgram_,"uDecal");
    meshUniforms_.decalMultiply=uniformLocation(meshProgram_,"uDecalMultiply");
    meshUniforms_.decalAdditive=uniformLocation(meshProgram_,"uDecalAdditive");
    meshUniforms_.shadowSpecularMultiplier=uniformLocation(meshProgram_,"uShadowSpecularMultiplier");
    meshUniforms_.alphaTest=uniformLocation(meshProgram_,"uAlphaTest");
    meshUniforms_.hasSpecularImperfections=uniformLocation(meshProgram_,"uHasSpecularImperfections");
    meshUniforms_.camoBlend=uniformLocation(meshProgram_,"uCamoBlend");
    meshUniforms_.camoUseAlpha=uniformLocation(meshProgram_,"uCamoUseAlpha");
    meshUniforms_.camoMaskUseful=uniformLocation(meshProgram_,"uCamoMaskUseful");
    meshUniforms_.viewmodelLightViewProjection=uniformLocation(meshProgram_,"uViewmodelLightViewProjection");
    meshUniforms_.weaponSurface=uniformLocation(meshProgram_,"uWeaponSurface");
    meshUniforms_.weaponSpecularMultiplier=uniformLocation(meshProgram_,"uWeaponSpecularMultiplier");
    meshUniforms_.weaponSpecularLow=uniformLocation(meshProgram_,"uWeaponSpecularLow");
    meshUniforms_.weaponSpecularHigh=uniformLocation(meshProgram_,"uWeaponSpecularHigh");
    meshUniforms_.weaponCubemapMultiplier=uniformLocation(meshProgram_,"uWeaponCubemapMultiplier");
    meshUniforms_.specularIntensity=uniformLocation(meshProgram_,"uSpecularIntensity");
    meshUniforms_.specularSharpness=uniformLocation(meshProgram_,"uSpecularSharpness");
    meshUniforms_.specularIntensity2=uniformLocation(meshProgram_,"uSpecularIntensity2");
    meshUniforms_.specularSharpness2=uniformLocation(meshProgram_,"uSpecularSharpness2");
    meshUniforms_.lensAlpha=uniformLocation(meshProgram_,"uLensAlpha");
    meshUniforms_.lensTint=uniformLocation(meshProgram_,"uLensTint");
    meshUniforms_.lensTintIntensity=uniformLocation(meshProgram_,"uLensTintIntensity");
    meshUniforms_.lensSpecularIntensity=uniformLocation(meshProgram_,"uLensSpecularIntensity");
    meshUniforms_.lensCubemapIntensity=uniformLocation(meshProgram_,"uLensCubemapIntensity");
    meshUniforms_.eeveeMetallic=uniformLocation(meshProgram_,"uEeveeMetallic");
    meshUniforms_.eeveeRoughness=uniformLocation(meshProgram_,"uEeveeRoughness");
    meshUniforms_.eeveeIor=uniformLocation(meshProgram_,"uEeveeIor");
    meshUniforms_.eeveeSpecular=uniformLocation(meshProgram_,"uEeveeSpecular");
    meshUniforms_.eeveeSpecularTint=uniformLocation(meshProgram_,"uEeveeSpecularTint");
    meshUniforms_.eeveeClearcoat=uniformLocation(meshProgram_,"uEeveeClearcoat");
    meshUniforms_.eeveeClearcoatRoughness=uniformLocation(meshProgram_,"uEeveeClearcoatRoughness");
    meshUniforms_.brdfModel=uniformLocation(meshProgram_,"uBrdfModel");

    shadowUniforms_.lightViewProjection=uniformLocation(shadowProgram_,"uLightViewProjection");
    shadowUniforms_.bones=uniformLocation(shadowProgram_,"uBones");
    shadowUniforms_.boneVisibility=uniformLocation(shadowProgram_,"uBoneVisibility");
    shadowUniforms_.model=uniformLocation(shadowProgram_,"uModel");
    shadowUniforms_.skinned=uniformLocation(shadowProgram_,"uSkinned");
#ifdef _WIN32
    const auto comResult=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);ownsCom_=SUCCEEDED(comResult);
    IWICImagingFactory* factory{};
    auto factoryResult=CoCreateInstance(CLSID_WICImagingFactory2,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(FAILED(factoryResult))factoryResult=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(factoryResult))imageFactory_=factory;
#endif
    initialized_=true;return true;
}

void StageRenderer::releaseTarget(){
    if(volumetricTexture_)glDeleteTextures(1,&volumetricTexture_);volumetricTexture_=0;
    if(volumetricFramebuffer_)glapi::DeleteFramebuffers(1,&volumetricFramebuffer_);volumetricFramebuffer_=0;volumetricWidth_=volumetricHeight_=0;
    for(auto& texture:hbaoTextures_)if(texture){glDeleteTextures(1,&texture);texture=0;}
    if(hbaoFramebuffer_)glapi::DeleteFramebuffers(1,&hbaoFramebuffer_);hbaoFramebuffer_=0;
    for(auto& t:dofTextures_)if(t){glDeleteTextures(1,&t);t=0;}if(dofFramebuffer_)glapi::DeleteFramebuffers(1,&dofFramebuffer_);dofFramebuffer_=0;dofWidth_=dofHeight_=0;
    if(aoIsolationTexture_)glDeleteTextures(1,&aoIsolationTexture_);if(aoIsolationFramebuffer_)glapi::DeleteFramebuffers(1,&aoIsolationFramebuffer_);aoIsolationTexture_=aoIsolationFramebuffer_=0;
    if(depthBuffer_)glDeleteTextures(1,&depthBuffer_);if(colorTexture_)glDeleteTextures(1,&colorTexture_);if(postTexture_)glDeleteTextures(1,&postTexture_);for(auto& texture:bloomTextures_)if(texture){glDeleteTextures(1,&texture);texture=0;}
    if(framebuffer_)glapi::DeleteFramebuffers(1,&framebuffer_);if(postFramebuffer_)glapi::DeleteFramebuffers(1,&postFramebuffer_);if(bloomFramebuffer_)glapi::DeleteFramebuffers(1,&bloomFramebuffer_);depthBuffer_=colorTexture_=framebuffer_=postTexture_=postFramebuffer_=bloomFramebuffer_=0;bloomWidths_={};bloomHeights_={};width_=height_=0;
}

#include "render/HbaoPass.inc"
#include "render/VolumetricLightingPass.inc"
#include "render/WaterPass.inc"
#include "render/DepthOfFieldPass.inc"

void StageRenderer::releaseMsaaTarget(){
    if(msaaDepthBuffer_)glapi::DeleteRenderbuffers(1,&msaaDepthBuffer_);if(msaaColorBuffer_)glapi::DeleteRenderbuffers(1,&msaaColorBuffer_);if(msaaFramebuffer_)glapi::DeleteFramebuffers(1,&msaaFramebuffer_);msaaDepthBuffer_=msaaColorBuffer_=msaaFramebuffer_=0;msaaWidth_=msaaHeight_=0;
}

void StageRenderer::clearScene(){
    overlayHistory_.actors.clear();overlayImpacts_.clear();
    for(auto& p:poseUploads_){if(p.boneTexture)glDeleteTextures(1,&p.boneTexture);if(p.visibilityTexture)glDeleteTextures(1,&p.visibilityTexture);if(p.bones)glapi::DeleteBuffers(1,&p.bones);if(p.visibility)glapi::DeleteBuffers(1,&p.visibility);}
    poseUploads_.clear();
    for(auto& mesh:meshes_){if(mesh.wireIndexBuffer)glapi::DeleteBuffers(1,&mesh.wireIndexBuffer);if(mesh.indexBuffer)glapi::DeleteBuffers(1,&mesh.indexBuffer);if(mesh.vertexBuffer)glapi::DeleteBuffers(1,&mesh.vertexBuffer);if(mesh.vao)glapi::DeleteVertexArrays(1,&mesh.vao);}
    meshes_.clear();mainMeshCount_=mapMeshFirst_=mapMeshCount_=worldActorMeshFirst_=worldActorMeshCount_=actorMeshFirst_=actorMeshCount_=0;classMainFirst_={};classMainCount_={};classWorldFirst_={};classWorldCount_={};classScenesResident_=false;activeClassSlot_=0;
    mapOpaqueMeshes_.clear();mapDecalMeshes_.clear();mapTransparentMeshes_.clear();mapAdditiveMeshes_.clear();mapShadowCasterMeshes_.clear();
    for(const auto texture:textures_)if(texture){textureHasUsefulAlpha_.erase(texture);glDeleteTextures(1,&texture);}textures_.clear();sceneTextureCache_.clear();
}

void StageRenderer::shutdown(){
    if(nightPanorama_)glDeleteTextures(1,&nightPanorama_);nightPanorama_=0;nightPanoramaPath_.clear();
    driverPoll_={};driverTotalMb_=driverFreeMb_=0;
    if(volumetricProgram_)glapi::DeleteProgram(volumetricProgram_);volumetricProgram_=0;
    if(waterProgram_)glapi::DeleteProgram(waterProgram_);waterProgram_=0;
    if(waterSceneFramebuffer_)glapi::DeleteFramebuffers(1,&waterSceneFramebuffer_);waterSceneFramebuffer_=0;
    if(waterSceneDepth_)glDeleteTextures(1,&waterSceneDepth_);waterSceneDepth_=0;
    if(waterSceneColor_)glDeleteTextures(1,&waterSceneColor_);waterSceneColor_=0;
    if(waterCrestTexture_)glDeleteTextures(1,&waterCrestTexture_);waterCrestTexture_=0;waterCrestTightness_=-1.f;
    if(waterFoamTexture_)glDeleteTextures(1,&waterFoamTexture_);waterFoamTexture_=0;
    waterSceneWidth_=waterSceneHeight_=0;
    if(waterVao_)glapi::DeleteVertexArrays(1,&waterVao_);waterVao_=0;
    if(waterVertices_)glapi::DeleteBuffers(1,&waterVertices_);waterVertices_=0;
    if(waterIndices_)glapi::DeleteBuffers(1,&waterIndices_);waterIndices_=0;
    waterResolution_=waterIndexCount_=0;waterError_.clear();
    gpuTimer_.reset();
    uniformLocations_.clear();
    if(!initialized_)return;clearScene();clearCamoTexture();clearScopeOverlayTexture();clearMuzzleFlashTexture();clearSmokeTexture();clearImpactSurfaceTexture();clearImpactBotTexture();clearImpacts();clearBulletTrailTexture();clearSpecularImperfectionsTexture();clearEnvironment();clearSmokeCurve();releaseMsaaTarget();releaseTarget();
    if(shadowTexture_)glDeleteTextures(1,&shadowTexture_);if(shadowFramebuffer_)glapi::DeleteFramebuffers(1,&shadowFramebuffer_);if(viewmodelShadowTexture_)glDeleteTextures(1,&viewmodelShadowTexture_);if(viewmodelShadowFramebuffer_)glapi::DeleteFramebuffers(1,&viewmodelShadowFramebuffer_);if(farShadowTexture_)glDeleteTextures(1,&farShadowTexture_);if(farShadowFramebuffer_)glapi::DeleteFramebuffers(1,&farShadowFramebuffer_);
    if(boneTexture_)glDeleteTextures(1,&boneTexture_);if(boneBuffer_)glapi::DeleteBuffers(1,&boneBuffer_);if(visibilityTexture_)glDeleteTextures(1,&visibilityTexture_);if(visibilityBuffer_)glapi::DeleteBuffers(1,&visibilityBuffer_);
    if(lineBuffer_)glapi::DeleteBuffers(1,&lineBuffer_);if(lineVao_)glapi::DeleteVertexArrays(1,&lineVao_);
    if(billboardBuffer_)glapi::DeleteBuffers(1,&billboardBuffer_);if(billboardVao_)glapi::DeleteVertexArrays(1,&billboardVao_);
    if(meshProgram_)glapi::DeleteProgram(meshProgram_);if(lineProgram_)glapi::DeleteProgram(lineProgram_);if(billboardProgram_)glapi::DeleteProgram(billboardProgram_);if(shadowProgram_)glapi::DeleteProgram(shadowProgram_);if(environmentProgram_)glapi::DeleteProgram(environmentProgram_);if(postProgram_)glapi::DeleteProgram(postProgram_);if(hbaoProgram_)glapi::DeleteProgram(hbaoProgram_);hbaoProgram_=0;if(dofProgram_)glapi::DeleteProgram(dofProgram_);dofProgram_=0;if(kawaseProgram_)glapi::DeleteProgram(kawaseProgram_);clearLutTexture();
#ifdef _WIN32
    if(imageFactory_){static_cast<IWICImagingFactory*>(imageFactory_)->Release();imageFactory_=nullptr;}
    if(ownsCom_){CoUninitialize();ownsCom_=false;}
#endif
    boneTexture_=boneBuffer_=visibilityTexture_=visibilityBuffer_=lineBuffer_=lineVao_=billboardBuffer_=billboardVao_=meshProgram_=lineProgram_=billboardProgram_=shadowProgram_=environmentProgram_=shadowFramebuffer_=shadowTexture_=viewmodelShadowFramebuffer_=viewmodelShadowTexture_=farShadowFramebuffer_=farShadowTexture_=0;initialized_=false;
}

bool StageRenderer::setNightSkyPanorama(const std::filesystem::path& path){
    if(path==nightPanoramaPath_)return nightPanorama_!=0;
    if(nightPanorama_)glDeleteTextures(1,&nightPanorama_);nightPanorama_=0;
    nightPanoramaPath_=path;
    nightPanorama_=loadTexture(path,false,true,6000,false,true);
    if(nightPanorama_){glBindTexture(GL_TEXTURE_2D,nightPanorama_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);}
    return nightPanorama_!=0;
}
unsigned StageRenderer::loadTexture(const std::filesystem::path& path,bool sceneTexture,bool forceOpaque,int maxDimension,bool compress,bool srgb) {
#ifdef _WIN32
    if(!imageFactory_||path.empty()||!std::filesystem::exists(path))return 0;
    const GLenum kCompressedRgbDxt1 = srgb?0x8C4C:0x83F0;
    const GLenum kCompressedRgbaDxt5 = srgb?0x8C4F:0x83F3;
    const GLenum rgbaInternal = srgb?0x8C43:glapi::Rgba8;
    const auto usefulAlpha=[](const std::uint8_t* pixels,std::size_t pixelCount){std::uint8_t minimum=255,maximum=0;std::size_t nonOpaque{};for(std::size_t i=0;i<pixelCount;++i){const auto alpha=pixels[i*4+3];minimum=std::min(minimum,alpha);maximum=std::max(maximum,alpha);if(alpha<250)++nonOpaque;}return maximum-minimum>=16&&nonOpaque>pixelCount/1000;};
    if(path.extension()==L".webp"){
        std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)return 0;const auto size=input.tellg();if(size<=0)return 0;input.seekg(0);std::vector<std::uint8_t> encoded(static_cast<std::size_t>(size));input.read(reinterpret_cast<char*>(encoded.data()),size);if(!input)return 0;
        int width{},height{};if(!WebPGetInfo(encoded.data(),encoded.size(),&width,&height)||width<=0||height<=0)return 0;GLint maxSize{};glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);if(width>maxSize||height>maxSize)return 0;auto* pixels=WebPDecodeRGBA(encoded.data(),encoded.size(),&width,&height);if(!pixels)return 0;
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width)*height*4);std::memcpy(rgba.data(),pixels,rgba.size());WebPFree(pixels);
        downscaleRgba(rgba,width,height,maxDimension);
        const bool hasUsefulAlpha=!forceOpaque&&usefulAlpha(rgba.data(),static_cast<std::size_t>(width)*height);if(forceOpaque)for(std::size_t i=0;i<static_cast<std::size_t>(width)*height;++i)rgba[i*4+3]=255;
        unsigned texture{};glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        const GLenum internalFormat = compress ? (hasUsefulAlpha ? kCompressedRgbaDxt5 : kCompressedRgbDxt1) : rgbaInternal;
        glTexImage2D(GL_TEXTURE_2D,0,internalFormat,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        if(glapi::GenerateMipmap)glapi::GenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glapi::GenerateMipmap?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);textureHasUsefulAlpha_[texture]=hasUsefulAlpha;if(sceneTexture)textures_.push_back(texture);return texture;
    }
    const auto ext=path.extension();
    if(ext==L".tga"||ext==L".TGA"){
        TgaImage tga;
        if(loadTga(path,tga,maxDimension)){
            GLint maxSize{};glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);
            if(tga.width<=maxSize&&tga.height<=maxSize){
                unsigned texture{};glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                if(forceOpaque)for(std::size_t i=3;i<tga.rgba.size();i+=4)tga.rgba[i]=255;
                const bool hasUsefulAlpha=!forceOpaque&&tga.hasUsefulAlpha;
                const GLenum internalFormat = compress ? (hasUsefulAlpha ? kCompressedRgbaDxt5 : kCompressedRgbDxt1) : rgbaInternal;
                glTexImage2D(GL_TEXTURE_2D,0,internalFormat,tga.width,tga.height,0,GL_RGBA,GL_UNSIGNED_BYTE,tga.rgba.data());
                if(glapi::GenerateMipmap)glapi::GenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glapi::GenerateMipmap?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
                textureHasUsefulAlpha_[texture]=hasUsefulAlpha;
                if(sceneTexture)textures_.push_back(texture);
                return texture;
            }
        }
    }
    if(ext==L".dds"||ext==L".DDS"){
        dds::Image ddsImg;std::string ddsErr;
        if(dds::loadFile(path,ddsImg,ddsErr)){
            GLint maxSize{};glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);
            if(ddsImg.width<=static_cast<UINT>(maxSize)&&ddsImg.height<=static_cast<UINT>(maxSize)){
                unsigned texture{};glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                if(ddsImg.isCompressed&&glapi::CompressedTexImage2D&&ddsImg.glInternalFormat){
                    glapi::CompressedTexImage2D(GL_TEXTURE_2D,0,ddsImg.glInternalFormat,ddsImg.width,ddsImg.height,0,static_cast<GLsizei>(ddsImg.topMipByteSize),ddsImg.data.data());
                }else{
                    std::vector<std::uint8_t> rgba;
                    if(dds::decompressTopMipToRgba(ddsImg,rgba)){
                        int w=static_cast<int>(ddsImg.width),h=static_cast<int>(ddsImg.height);
                        downscaleRgba(rgba,w,h,maxDimension);
                        const GLenum internalFormat = compress ? (ddsImg.hasUsefulAlpha ? kCompressedRgbaDxt5 : kCompressedRgbDxt1) : rgbaInternal;
                        glTexImage2D(GL_TEXTURE_2D,0,internalFormat,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
                    }else if(!ddsImg.isCompressed&&!ddsImg.data.empty()){
                        int w=static_cast<int>(ddsImg.width),h=static_cast<int>(ddsImg.height);
                        downscaleRgba(ddsImg.data,w,h,maxDimension);
                        const GLenum internalFormat = compress ? (ddsImg.hasUsefulAlpha ? kCompressedRgbaDxt5 : kCompressedRgbDxt1) : (ddsImg.glInternalFormat?ddsImg.glInternalFormat:glapi::Rgba8);
                        glTexImage2D(GL_TEXTURE_2D,0,internalFormat,w,h,0,ddsImg.glFormat?ddsImg.glFormat:GL_RGBA,ddsImg.glType?ddsImg.glType:GL_UNSIGNED_BYTE,ddsImg.data.data());
                    }
                }
                if(!ddsImg.isCompressed&&glapi::GenerateMipmap)glapi::GenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,(!ddsImg.isCompressed&&glapi::GenerateMipmap)?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
                textureHasUsefulAlpha_[texture]=ddsImg.hasUsefulAlpha;
                if(sceneTexture)textures_.push_back(texture);
                return texture;
            }
        }
    }
    using Microsoft::WRL::ComPtr;
    auto* factory=static_cast<IWICImagingFactory*>(imageFactory_);
    ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)))return 0;
    if(FAILED(decoder->GetFrame(0,&frame))||FAILED(factory->CreateFormatConverter(&converter)))return 0;
    if(FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return 0;
    UINT width{},height{};if(FAILED(converter->GetSize(&width,&height))||!width||!height)return 0;
    GLint maxSize{};glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);if(width>static_cast<UINT>(maxSize)||height>static_cast<UINT>(maxSize))return 0;
    const UINT kMaxDim=static_cast<UINT>(maxDimension);
    UINT targetWidth=width,targetHeight=height;
    while(targetWidth>kMaxDim||targetHeight>kMaxDim){targetWidth=std::max(1u,targetWidth/2);targetHeight=std::max(1u,targetHeight/2);}
    if(!forceOpaque&&(targetWidth!=width||targetHeight!=height)){
        ComPtr<IWICBitmapScaler> scaler;
        if(SUCCEEDED(factory->CreateBitmapScaler(&scaler))&&SUCCEEDED(scaler->Initialize(frame.Get(),targetWidth,targetHeight,WICBitmapInterpolationModeLinear))){
            converter=nullptr;
            if(SUCCEEDED(factory->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))){
                width=targetWidth;height=targetHeight;
            }
        }
    }
    const auto stride=static_cast<std::size_t>(width)*4,byteCount=stride*height;
    if(byteCount>static_cast<std::size_t>(std::numeric_limits<UINT>::max()))return 0;
    std::vector<std::uint8_t> pixels(byteCount);
    if(FAILED(converter->CopyPixels(nullptr,static_cast<UINT>(stride),static_cast<UINT>(byteCount),pixels.data())))return 0;
    if(forceOpaque){for(std::size_t i=3;i<pixels.size();i+=4)pixels[i]=255;int opaqueWidth=static_cast<int>(width),opaqueHeight=static_cast<int>(height);downscaleRgba(pixels,opaqueWidth,opaqueHeight,maxDimension);width=static_cast<UINT>(opaqueWidth);height=static_cast<UINT>(opaqueHeight);}
    const bool hasUsefulAlpha=!forceOpaque&&usefulAlpha(pixels.data(),static_cast<std::size_t>(width)*height);
    unsigned texture{};glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    const GLenum internalFormat = compress ? (hasUsefulAlpha ? kCompressedRgbaDxt5 : kCompressedRgbDxt1) : rgbaInternal;
    glTexImage2D(GL_TEXTURE_2D,0,internalFormat,static_cast<GLsizei>(width),static_cast<GLsizei>(height),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    if(glapi::GenerateMipmap)glapi::GenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glapi::GenerateMipmap?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    textureHasUsefulAlpha_[texture]=hasUsefulAlpha;if(sceneTexture)textures_.push_back(texture);return texture;
#else
    (void)path;(void)sceneTexture;(void)forceOpaque;(void)maxDimension;(void)compress;return 0;
#endif
}

unsigned StageRenderer::uploadPreparedTexture(const texture::Prepared& prepared,double* transferMs,double* mipmapMs){
    const auto start=std::chrono::steady_clock::now();
    unsigned texture{};glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    if(prepared.unpackOne)glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(prepared.compressed)glapi::CompressedTexImage2D(GL_TEXTURE_2D,0,prepared.internalFormat,prepared.width,prepared.height,0,static_cast<GLsizei>(prepared.bytes.size()),prepared.bytes.data());
    else glTexImage2D(GL_TEXTURE_2D,0,prepared.internalFormat,prepared.width,prepared.height,0,prepared.format,prepared.type,prepared.bytes.data());
    const auto transferEnd=std::chrono::steady_clock::now();
    if(transferMs)*transferMs+=std::chrono::duration<double,std::milli>(transferEnd-start).count();
    const bool mipmaps=prepared.mipmaps&&glapi::GenerateMipmap;
    if(mipmaps)glapi::GenerateMipmap(GL_TEXTURE_2D);
    if(mipmapMs)*mipmapMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-transferEnd).count();
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,mipmaps?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    textureHasUsefulAlpha_[texture]=prepared.usefulAlpha;textures_.push_back(texture);return texture;
}

bool StageRenderer::setCamoTexture(const std::filesystem::path& path,std::string& error){
    error.clear();const auto texture=loadTexture(path,false);if(!texture){error="Could not load camo texture "+path.filename().string();return false;}
    clearCamoTexture();camoTexture_=texture;return true;
}
bool StageRenderer::setLutTexture(const std::filesystem::path& path,std::string& error){error.clear();const auto texture=loadTexture(path,false);if(!texture){error="Could not load LUT "+path.filename().string();return false;}glBindTexture(GL_TEXTURE_2D,texture);GLint width{},height{};glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);if(height<2||width!=height*height){glDeleteTextures(1,&texture);error="LUT must be an N² by N strip image (for example 1024x32)";return false;}clearLutTexture();lutTexture_=texture;lutSize_=height;return true;}
void StageRenderer::clearLutTexture(){if(lutTexture_)glDeleteTextures(1,&lutTexture_);lutTexture_=0;lutSize_=0;}

void StageRenderer::clearCamoTexture(){if(camoTexture_){textureHasUsefulAlpha_.erase(camoTexture_);glDeleteTextures(1,&camoTexture_);}camoTexture_=0;}

bool StageRenderer::setSpecularImperfectionsTexture(const std::filesystem::path& path,std::string& error){error.clear();const auto texture=loadTexture(path,false);if(!texture){error="Could not load specular imperfections "+path.filename().string();return false;}clearSpecularImperfectionsTexture();specularImperfectionsTexture_=texture;return true;}
void StageRenderer::clearSpecularImperfectionsTexture(){if(specularImperfectionsTexture_)glDeleteTextures(1,&specularImperfectionsTexture_);specularImperfectionsTexture_=0;}

bool StageRenderer::setScopeOverlayTexture(const std::filesystem::path& path,std::string& error){
    error.clear();unsigned texture{};float loadedAspect=1.0f;auto extension=path.extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(extension==".iwi"){
        std::ifstream input(path,std::ios::binary|std::ios::ate);const auto length=input?input.tellg():std::streampos{};if(!input||length<48){error="Scope IWI is invalid";return false;}input.seekg(0);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));input.read(reinterpret_cast<char*>(bytes.data()),length);if(!input||bytes[0]!='I'||bytes[1]!='W'||bytes[2]!='i'){error="Not an Infinity Ward Image";return false;}
        const auto u16=[&](std::size_t at){return static_cast<unsigned>(bytes[at]|(static_cast<unsigned>(bytes[at+1])<<8));};const unsigned format=bytes[4],width=u16(6),height=u16(8);std::size_t offset=bytes[3]==0x0d?48:40;if(!width||!height||offset>=bytes.size()){error="Scope IWI dimensions are invalid";return false;}GLenum compressed{};std::size_t expected{};if(format==0x0b){compressed=0x83F1;expected=((width+3)/4)*((height+3)/4)*8;}else if(format==0x0c){compressed=0x83F2;expected=((width+3)/4)*((height+3)/4)*16;}else if(format==0x0d){compressed=0x83F3;expected=((width+3)/4)*((height+3)/4)*16;}else {error="Unsupported scope IWI format "+std::to_string(format);return false;}if(bytes.size()-offset<expected){error="Scope IWI top mip is truncated";return false;}glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glapi::CompressedTexImage2D(GL_TEXTURE_2D,0,compressed,width,height,0,static_cast<GLsizei>(expected),bytes.data()+offset);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);
        loadedAspect=static_cast<float>(width)/std::max(1u,height);
    }else texture=loadTexture(path,false);if(!texture){error="Could not load scope overlay "+path.filename().string();return false;}
    glBindTexture(GL_TEXTURE_2D,texture);GLint imageWidth{},imageHeight{};glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&imageWidth);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&imageHeight);
    scene::Vec3 edge{};if(imageWidth>0&&imageHeight>0&&imageWidth<=16384&&imageHeight<=16384){std::vector<unsigned char> pixels(static_cast<std::size_t>(imageWidth)*imageHeight*4);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());std::size_t count=0;for(int y=0;y<imageHeight;++y)for(int x=0;x<imageWidth;++x)if(x==0||y==0||x==imageWidth-1||y==imageHeight-1){const auto at=(static_cast<std::size_t>(y)*imageWidth+x)*4;edge+=scene::Vec3{float(pixels[at]),float(pixels[at+1]),float(pixels[at+2])};++count;}edge=edge/(255.f*std::max<std::size_t>(1,count));loadedAspect=float(imageWidth)/imageHeight;}
    clearScopeOverlayTexture();scopeOverlayTexture_=texture;scopeOverlayAspect_=loadedAspect;scopeOverlayEdgeColor_=edge;return true;
}

void StageRenderer::clearScopeOverlayTexture(){if(scopeOverlayTexture_)glDeleteTextures(1,&scopeOverlayTexture_);scopeOverlayTexture_=0;scopeOverlayAspect_=1.0f;}

bool StageRenderer::setMuzzleFlashTexture(const std::filesystem::path& path,std::string& error){
    error.clear();
    const auto texture=loadTexture(path,false);
    if(!texture){
        error="Could not load muzzle flash "+path.filename().string();
        return false;
    }
    clearMuzzleFlashTexture();
    muzzleFlashTexture_=texture;
    return true;
}

void StageRenderer::clearMuzzleFlashTexture(){
    if(muzzleFlashTexture_)glDeleteTextures(1,&muzzleFlashTexture_);
    muzzleFlashTexture_=0;
}

bool StageRenderer::setSmokeTexture(const std::filesystem::path& path,std::string& error){
    error.clear();
    const auto texture=loadTexture(path,false);
    if(!texture){
        error="Could not load smoke texture "+path.filename().string();
        return false;
    }
    clearSmokeTexture();
    smokeTexture_=texture;
    return true;
}

void StageRenderer::clearSmokeTexture(){
    if(smokeTexture_)glDeleteTextures(1,&smokeTexture_);
    smokeTexture_=0;
}

void StageRenderer::clearEnvironment(){
    if(environmentTexture_)glDeleteTextures(1,&environmentTexture_);environmentTexture_=0;
    for(auto& face:environmentFaces_){if(face)glDeleteTextures(1,&face);face=0;}environmentMode_=0;
}

bool StageRenderer::setEnvironmentPanorama(const std::filesystem::path& path,std::string& error){
    error.clear();unsigned texture{};auto extension=path.extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(extension==".hdr"){
        std::ifstream input(path,std::ios::binary);std::string line;if(!input||!std::getline(input,line)||line.rfind("#?",0)!=0){error="Not a Radiance HDR image";return false;}int width{},height{};while(std::getline(input,line)){if(line.empty()||line=="\r")continue;char ySign{},yAxis{},xSign{},xAxis{};std::istringstream dimensions(line);if(dimensions>>ySign>>yAxis>>height>>xSign>>xAxis>>width&&std::toupper(yAxis)=='Y'&&std::toupper(xAxis)=='X')break;}if(width<=0||height<=0||width>32768||height>32768){error="HDR dimensions are invalid";return false;}
        std::vector<float> pixels(static_cast<std::size_t>(width)*height*3);std::vector<unsigned char> scanline(static_cast<std::size_t>(width)*4);for(int y=0;y<height;++y){unsigned char marker[4]{};input.read(reinterpret_cast<char*>(marker),4);if(!input||marker[0]!=2||marker[1]!=2||((marker[2]<<8)|marker[3])!=width){error="Unsupported legacy HDR scanline";return false;}for(int channel=0;channel<4;++channel){int x=0;while(x<width){unsigned char count{},value{};input.read(reinterpret_cast<char*>(&count),1);input.read(reinterpret_cast<char*>(&value),1);if(!input||count==0){error="Truncated HDR scanline";return false;}if(count>128){const int run=count-128;if(x+run>width){error="Invalid HDR run";return false;}for(int i=0;i<run;++i)scanline[static_cast<std::size_t>(x+i)*4+channel]=value;x+=run;}else{const int literal=count;if(x+literal>width){error="Invalid HDR literal";return false;}scanline[static_cast<std::size_t>(x)*4+channel]=value;for(int i=1;i<literal;++i){unsigned char sample{};input.read(reinterpret_cast<char*>(&sample),1);scanline[static_cast<std::size_t>(x+i)*4+channel]=sample;}if(!input){error="Truncated HDR literal";return false;}x+=literal;}}}for(int x=0;x<width;++x){const auto* rgbe=&scanline[static_cast<std::size_t>(x)*4];const float scale=rgbe[3]?std::ldexp(1.0f,static_cast<int>(rgbe[3])-136):0.0f;const auto target=(static_cast<std::size_t>(y)*width+x)*3;pixels[target]=rgbe[0]*scale;pixels[target+1]=rgbe[1]*scale;pixels[target+2]=rgbe[2]*scale;}}
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgb16f,width,height,0,GL_RGB,GL_FLOAT,pixels.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);
    }else texture=loadTexture(path,false);
    if(!texture){error="Could not load environment image "+path.filename().string();return false;}clearEnvironment();environmentTexture_=texture;environmentMode_=1;return true;
}

bool StageRenderer::setT6SkyboxIwi(const std::filesystem::path& path,std::string& error){
    error.clear();std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input){error="Could not open IWI";return false;}const auto length=input.tellg();if(length<0x28){error="IWI is too small";return false;}input.seekg(0);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));input.read(reinterpret_cast<char*>(bytes.data()),length);if(!input||bytes[0]!='I'||bytes[1]!='W'||bytes[2]!='i'){error="Not an Infinity Ward Image";return false;}if(bytes[3]!=0x1b){error="T6 sky requires IWI version 27";return false;}const auto u16=[&](std::size_t at){return static_cast<unsigned>(bytes[at]|(static_cast<unsigned>(bytes[at+1])<<8));};const auto u32=[&](std::size_t at){return static_cast<std::uint32_t>(bytes[at])|(static_cast<std::uint32_t>(bytes[at+1])<<8)|(static_cast<std::uint32_t>(bytes[at+2])<<16)|(static_cast<std::uint32_t>(bytes[at+3])<<24);};const unsigned format=bytes[4],usage=bytes[5],width=u16(6),height=u16(8);if(!width||width!=height){error="T6 cubemap dimensions are invalid";return false;}
    std::size_t faceBytes{};GLenum compressedFormat{};if(format==0x0b){faceBytes=((width+3)/4)*((height+3)/4)*8;compressedFormat=0x83F1;}else if(format==0x0c){faceBytes=((width+3)/4)*((height+3)/4)*16;compressedFormat=0x83F2;}else if(format==0x0d){faceBytes=((width+3)/4)*((height+3)/4)*16;compressedFormat=0x83F3;}else if(format==0x01)faceBytes=static_cast<std::size_t>(width)*height*4;else if(format==0x02)faceBytes=static_cast<std::size_t>(width)*height*3;else {error="Unsupported T6 IWI pixel format "+std::to_string(format);return false;}const std::size_t topOffset=u32(0x24);if(topOffset<0x28||topOffset>=bytes.size()){error="T6 IWI top-mip offset is invalid";return false;}const std::size_t available=bytes.size()-topOffset;if(available%6){error="T6 IWI cubemap payload is not divisible into six faces";return false;}const std::size_t storedFaceBytes=available/6;if(!storedFaceBytes||storedFaceBytes>faceBytes){error="T6 IWI cubemap face payload is invalid";return false;}std::vector<std::uint8_t> topStorage(faceBytes*6);for(std::size_t face=0;face<6;++face)std::copy_n(bytes.data()+topOffset+face*storedFaceBytes,storedFaceBytes,topStorage.data()+face*faceBytes);std::array<unsigned,6> loaded{};
    for(std::size_t face=0;face<6;++face){const auto* source=topStorage.data()+face*faceBytes;glGenTextures(1,&loaded[face]);glBindTexture(GL_TEXTURE_2D,loaded[face]);if(compressedFormat)glapi::CompressedTexImage2D(GL_TEXTURE_2D,0,compressedFormat,width,height,0,static_cast<GLsizei>(faceBytes),source);else {std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width)*height*4,255);for(std::size_t pixel=0;pixel<static_cast<std::size_t>(width)*height;++pixel){rgba[pixel*4]=source[pixel*(format==1?4:3)+2];rgba[pixel*4+1]=source[pixel*(format==1?4:3)+1];rgba[pixel*4+2]=source[pixel*(format==1?4:3)];if(format==1)rgba[pixel*4+3]=source[pixel*4+3];}glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());}glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);}
    clearEnvironment();environmentFaces_=loaded;environmentMode_=2;if(usage!=5)error="Loaded IWI cubemap (usage byte is not marked skybox)";return true;
}

bool StageRenderer::appendSceneMeshes(const scene::CastScene& scene,std::string& error,std::size_t firstSourceMesh,bool generateWireframe,bool isMapScene,ProgressCallback progressCallback) {
    if(!initialized_){error="Renderer is not initialized";return false;}
    const int maxTexDim = isMapScene ? mapTextureResolution_ : 1024;
    const bool compress = isMapScene && hardwareTextureCompression_;
    const std::size_t totalSourceMeshes = scene.meshes.size();
    if(isMapScene && progressCallback){
        char buf[160];
        std::snprintf(buf, sizeof(buf), "[GPU Upload] Initializing GPU buffers for %zu map submeshes...", totalSourceMeshes);
        progressCallback(buf, 0.92f);
        glfwPollEvents();
    }
    const std::size_t initialCacheSize = sceneTextureCache_.size();
    std::size_t lastReportedMesh = 0;
    std::size_t lastReportedTextures = 0;
    // Preserve the exact upload/cache-key order. Only value-owned CPU requests
    // reach workers; GL handles and the renderer's WIC factory stay here.
    std::vector<texture::Request> textureRequests;
    std::vector<std::wstring> textureKeys;
    std::unordered_set<std::wstring> queuedTextureKeys;
    GLint maxGpuDimension{};glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxGpuDimension);
    const auto queueTexture=[&](const std::filesystem::path& path,bool opaque,bool srgb){
        if(path.empty())return;
        const auto key=path.lexically_normal().wstring()+(opaque?L"|opaque":L"")+(srgb?L"|srgb":L"")+(compress?L"|compressed":L"")+L"|"+std::to_wstring(maxTexDim);
        if(sceneTextureCache_.contains(key)||!queuedTextureKeys.insert(key).second)return;
        textureKeys.push_back(key);textureRequests.push_back({path,opaque,compress,srgb,maxTexDim,maxGpuDimension,glapi::CompressedTexImage2D!=nullptr});
    };
    for(std::size_t i=std::min(firstSourceMesh,scene.meshes.size());i<scene.meshes.size();++i){
        const auto& source=scene.meshes[i];if(source.vertices.empty()||source.indices.empty())continue;
        const bool opaque=(source.ignoreAlbedoAlpha||source.viewmodelWeapon||source.camoBlend)&&!source.forceAlpha&&!source.decal&&!source.alphaTest;
        queueTexture(source.albedoPath,opaque,source.materialPolicyExplicit);
        for(const auto* path:{&source.normalPath,&source.specularPath,&source.metalnessPath,&source.roughnessPath})queueTexture(*path,false,false);
        queueTexture(source.emissivePath,false,source.materialPolicyExplicit);
    }
    // CPU elapsed timings only: no glFinish or blocking GPU queries.
    std::atomic<double> prepareMs{0};double transferMs=0,mipmapMs=0,waitMs=0,fallbackMs=0;
    std::size_t preparedCount=0,fallbackCount=0,preparedBytes=0;
    const auto measuredPrepare=[&](const texture::Request& request){
        const auto begin=std::chrono::steady_clock::now();auto result=texture::prepare(request);
        prepareMs.fetch_add(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count(),std::memory_order_relaxed);
        return result;
    };
    const auto fallbackLoad=[&](const std::filesystem::path& path,bool opaque,bool srgb){
        const auto begin=std::chrono::steady_clock::now();auto texture=loadTexture(path,true,opaque,maxTexDim,compress,srgb);
        fallbackMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();++fallbackCount;return texture;
    };
    using PreparationQueue=texture::BoundedPreparationQueue<texture::Request,texture::Prepared>;
    std::unique_ptr<PreparationQueue> preparation;
    // Small single-texture operations retain the direct loader's cheap path.
    if(textureRequests.size()>1&&imageFactory_)try{preparation=std::make_unique<PreparationQueue>(textureRequests,measuredPrepare);}catch(...){/* use original synchronous path */}
    std::size_t nextPrepared{};
    const auto loadPrepared=[&](const std::wstring& key,const std::filesystem::path& path,bool opaque,bool srgb){
        if(preparation&&nextPrepared<textureKeys.size()&&textureKeys[nextPrepared]==key){
            unsigned texture{};
            try{
                const auto waitBegin=std::chrono::steady_clock::now();
                preparation->consume([&](const texture::Prepared& prepared){
                    waitMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-waitBegin).count();
                    if(prepared.ready){texture=uploadPreparedTexture(prepared,&transferMs,&mipmapMs);++preparedCount;preparedBytes+=prepared.bytes.size();}
                    else texture=fallbackLoad(path,opaque,srgb);
                },[]{glfwPollEvents();});
                ++nextPrepared;return texture;
            }catch(...){preparation.reset();}
        }
        return fallbackLoad(path,opaque,srgb);
    };
    for(std::size_t sourceIndex=std::min(firstSourceMesh,scene.meshes.size());sourceIndex<scene.meshes.size();++sourceIndex){const auto& source=scene.meshes[sourceIndex];
        if(source.vertices.empty()||source.indices.empty())continue;
        GpuMesh mesh;mesh.indexCount=static_cast<int>(source.indices.size());mesh.model=source.modelTransform;mesh.color=source.color;mesh.skinned=source.skinned;mesh.camoBlend=source.camoBlend;mesh.camoUseAlpha=source.camoUseAlpha;mesh.hideWhenCamo=source.hideWhenCamo;mesh.lens=source.lens;mesh.eyeOverlay=source.eyeOverlay;mesh.emissive=source.emissive;mesh.forceAlpha=source.forceAlpha;mesh.decal=source.decal;mesh.decalMultiply=source.decalMultiply;mesh.decalAdditive=source.decalAdditive;mesh.alphaTest=source.alphaTest;mesh.ignoreAlbedoAlpha=source.ignoreAlbedoAlpha;mesh.viewmodelWeapon=source.viewmodelWeapon;mesh.gltfPbr=source.gltfPbr;mesh.specularGlossiness=source.specularGlossiness;mesh.metallicFactor=source.metallicFactor;mesh.roughnessFactor=source.roughnessFactor;mesh.transmissionFactor=source.transmissionFactor;mesh.indexOfRefraction=source.indexOfRefraction;mesh.emissiveFactor=source.emissiveFactor;mesh.attachmentIndex=source.attachmentIndex;mesh.actorVariant=source.actorVariant;
        mesh.materialPolicyExplicit=source.materialPolicyExplicit;mesh.doubleSided=source.doubleSided;mesh.unlit=source.unlit;mesh.useVertexColor=source.useVertexColor;mesh.alphaCutoff=source.alphaCutoff;mesh.materialDepthBias=source.materialDepthBias;mesh.renderQueue=source.renderQueue;mesh.sourceBlend=source.sourceBlend;mesh.destinationBlend=source.destinationBlend;
        mesh.aabbMin=source.bounds.minimum;mesh.aabbMax=source.bounds.maximum;mesh.hasBounds=source.bounds.valid;
        if(!mesh.hasBounds&&!source.vertices.empty()){
            mesh.aabbMin=source.vertices.front().position;mesh.aabbMax=source.vertices.front().position;
            for(const auto& v:source.vertices){
                mesh.aabbMin={std::min(mesh.aabbMin.x,v.position.x),std::min(mesh.aabbMin.y,v.position.y),std::min(mesh.aabbMin.z,v.position.z)};
                mesh.aabbMax={std::max(mesh.aabbMax.x,v.position.x),std::max(mesh.aabbMax.y,v.position.y),std::max(mesh.aabbMax.z,v.position.z)};
            }
            mesh.hasBounds=true;
        }
        mesh.sortKey={(mesh.aabbMin+mesh.aabbMax)*.5f,mesh.renderQueue<0?2000:mesh.renderQueue,mesh.materialPolicyExplicit};
        if(!isMapScene)mesh.cullBounds=visibility::MeshBounds::build(source,scene.skeleton.bones.size());
        else mesh.cullBounds.local={mesh.aabbMin,mesh.aabbMax,mesh.hasBounds};
        if(!source.albedoPath.empty()){
            const bool opaqueAlbedo=(source.ignoreAlbedoAlpha||source.viewmodelWeapon||source.camoBlend)&&!source.forceAlpha&&!source.decal&&!source.alphaTest;
            const auto key=source.albedoPath.lexically_normal().wstring()+(opaqueAlbedo?L"|opaque":L"")+(source.materialPolicyExplicit?L"|srgb":L"")+(compress?L"|compressed":L"")+L"|"+std::to_wstring(maxTexDim);
            const auto cached=sceneTextureCache_.find(key);
            if(cached!=sceneTextureCache_.end())mesh.texture=cached->second;
            else{
                mesh.texture=loadPrepared(key,source.albedoPath,opaqueAlbedo,source.materialPolicyExplicit);
                sceneTextureCache_.emplace(key,mesh.texture);
                const std::size_t bytesPerPixelNumerator = compress ? (opaqueAlbedo ? 2 : 4) : 16;
                estimatedVramBytes_ += static_cast<std::size_t>(maxTexDim) * maxTexDim * bytesPerPixelNumerator / 3;
            }
        }
        mesh.camoMaskUseful=mesh.texture&&textureHasUsefulAlpha_.contains(mesh.texture)&&textureHasUsefulAlpha_.at(mesh.texture);
        if(isMapScene && !mesh.materialPolicyExplicit && mesh.texture && textureHasUsefulAlpha_.contains(mesh.texture) && textureHasUsefulAlpha_.at(mesh.texture)){
            mesh.forceAlpha = true;
            mesh.alphaTest = true;
        }
        const auto loadCached=[&](const std::filesystem::path& path,bool srgb=false){
            if(path.empty())return 0u;
            const auto key=path.lexically_normal().wstring()+(srgb?L"|srgb":L"")+(compress?L"|compressed":L"")+L"|"+std::to_wstring(maxTexDim);
            if(const auto cached=sceneTextureCache_.find(key);cached!=sceneTextureCache_.end())return cached->second;
            const auto texture=loadPrepared(key,path,false,srgb);
            sceneTextureCache_.emplace(key,texture);
            const std::size_t bytesPerPixelNumerator = compress ? 4 : 16;
            estimatedVramBytes_ += static_cast<std::size_t>(maxTexDim) * maxTexDim * bytesPerPixelNumerator / 3;
            return texture;
        };
        mesh.normalTexture=loadCached(source.normalPath);
        mesh.specularTexture=loadCached(source.specularPath);
        mesh.metalnessTexture=loadCached(source.metalnessPath);
        mesh.source2Material=source.materialName.starts_with("weapons/models/")&&source.materialName.ends_with(".vmat");
        mesh.roughnessTexture=loadCached(source.roughnessPath);
        mesh.emissiveTexture=loadCached(source.emissivePath,source.materialPolicyExplicit);
        glapi::GenVertexArrays(1,&mesh.vao);glapi::GenBuffers(1,&mesh.vertexBuffer);glapi::GenBuffers(1,&mesh.indexBuffer);
        glapi::BindVertexArray(mesh.vao);glapi::BindBuffer(glapi::ArrayBuffer,mesh.vertexBuffer);
        glapi::BufferData(glapi::ArrayBuffer,static_cast<glapi::Size>(source.vertices.size()*sizeof(scene::Vertex)),source.vertices.data(),glapi::StaticDraw);
        glapi::BindBuffer(glapi::ElementArrayBuffer,mesh.indexBuffer);
        glapi::BufferData(glapi::ElementArrayBuffer,static_cast<glapi::Size>(source.indices.size()*sizeof(std::uint32_t)),source.indices.data(),glapi::StaticDraw);
        if(generateWireframe&&source.indices.size()<=65536){
            std::vector<std::uint32_t> wireIndices;wireIndices.reserve(source.indices.size()*2);
            for(std::size_t i=0;i+2<source.indices.size();i+=3){const auto a=source.indices[i],b=source.indices[i+1],c=source.indices[i+2];wireIndices.insert(wireIndices.end(),{a,b,b,c,c,a});}
            mesh.wireIndexCount=static_cast<int>(wireIndices.size());
            glapi::GenBuffers(1,&mesh.wireIndexBuffer);
            glapi::BindBuffer(glapi::ElementArrayBuffer,mesh.wireIndexBuffer);
            glapi::BufferData(glapi::ElementArrayBuffer,static_cast<glapi::Size>(wireIndices.size()*sizeof(std::uint32_t)),wireIndices.data(),glapi::StaticDraw);
            glapi::BindBuffer(glapi::ElementArrayBuffer,mesh.indexBuffer);
        }
        glapi::EnableVertexAttribArray(0);glapi::VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(scene::Vertex),reinterpret_cast<void*>(offsetof(scene::Vertex,position)));
        glapi::EnableVertexAttribArray(1);glapi::VertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(scene::Vertex),reinterpret_cast<void*>(offsetof(scene::Vertex,normal)));
        glapi::EnableVertexAttribArray(2);glapi::VertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(scene::Vertex),reinterpret_cast<void*>(offsetof(scene::Vertex,uv)));
        glapi::EnableVertexAttribArray(3);glapi::VertexAttribIPointer(3,4,GL_UNSIGNED_INT,sizeof(scene::Vertex),reinterpret_cast<void*>(offsetof(scene::Vertex,bones)));
        glapi::EnableVertexAttribArray(4);glapi::VertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(scene::Vertex),reinterpret_cast<void*>(offsetof(scene::Vertex,weights)));
        glapi::EnableVertexAttribArray(5);glapi::VertexAttribPointer(5,4,GL_FLOAT,GL_FALSE,sizeof(scene::Vertex),reinterpret_cast<void*>(offsetof(scene::Vertex,color)));
        meshes_.push_back(mesh);
        if(isMapScene){
            const std::size_t meshIdx = meshes_.size() - 1;
            if(mesh.decalAdditive) {
                mapAdditiveMeshes_.push_back(meshIdx);
            } else if(mesh.forceAlpha) {
                mapTransparentMeshes_.push_back(meshIdx);
            } else if(mesh.decal) {
                mapDecalMeshes_.push_back(meshIdx);
            } else {
                mapOpaqueMeshes_.push_back(meshIdx);
                mapShadowCasterMeshes_.push_back(meshIdx);
            }
        }
        if(isMapScene && progressCallback){
            const std::size_t currentTextures = sceneTextureCache_.size() - initialCacheSize;
            if(currentTextures >= lastReportedTextures + 120 || sourceIndex >= lastReportedMesh + 450 || sourceIndex + 1 == totalSourceMeshes){
                lastReportedMesh = sourceIndex;
                lastReportedTextures = currentTextures;
                const float submeshProgress = static_cast<float>(sourceIndex + 1) / std::max<std::size_t>(1, totalSourceMeshes);
                const float progress = 0.92f + submeshProgress * 0.06f;
                char buf[160];
                std::snprintf(buf, sizeof(buf), "[GPU Upload] Streaming textures to VRAM: %zu textures uploaded, %zu/%zu submeshes (%.0f%%)...",
                              currentTextures, sourceIndex + 1, totalSourceMeshes, submeshProgress * 100.0f);
                progressCallback(buf, progress);
                glfwPollEvents();
            }
        }
    }
    if(isMapScene){
        if(progressCallback){
            char buf[160];
            char timings[640];
            std::snprintf(timings,sizeof(timings),"[Map textures] Prepared %zu (%.1f MiB); worker preparation sum %.1f ms (parallel, non-additive); main-thread wait %.1f ms; upload/driver compression calls %.1f ms; mipmap calls %.1f ms; synchronous fallback %zu / %.1f ms. CPU elapsed, not GPU completion.",
                preparedCount,double(preparedBytes)/(1024*1024),prepareMs.load(),waitMs,transferMs,mipmapMs,fallbackCount,fallbackMs);
            progressCallback(timings,0.984f);
            std::snprintf(buf, sizeof(buf), "[GPU Upload] Sorting %zu submeshes into material buckets & caching shadow casters...", totalSourceMeshes);
            progressCallback(buf, 0.985f);
            glfwPollEvents();
        }
        std::sort(mapOpaqueMeshes_.begin(), mapOpaqueMeshes_.end(), [&](std::size_t a, std::size_t b) {
            if (meshes_[a].texture != meshes_[b].texture)
                return meshes_[a].texture < meshes_[b].texture;
            return meshes_[a].vao < meshes_[b].vao;
        });
        std::sort(mapShadowCasterMeshes_.begin(), mapShadowCasterMeshes_.end(), [&](std::size_t a, std::size_t b) {
            return meshes_[a].vao < meshes_[b].vao;
        });
        sortStaticOpaqueMaterialPass(mapOpaqueMeshes_,meshes_);
    }
    glapi::BindVertexArray(0);return true;
}

bool StageRenderer::loadScene(const scene::CastScene& scene,std::string& error) {
    clearScene();if(!appendSceneMeshes(scene,error))return false;mainMeshCount_=meshes_.size();classMainFirst_[0]=0;classMainCount_[0]=mainMeshCount_;return true;
}

bool StageRenderer::replaceMainScene(const scene::CastScene& scene, const scene::CastScene* worldActorScene, const scene::CastScene* botActorScene, std::string& error) {
    classScenesResident_ = false;
    if (mapMeshCount_ == 0) {
        if (!loadScene(scene, error)) return false;
        return loadAuxiliaryScenes(nullptr, worldActorScene, botActorScene, error);
    }
    while (meshes_.size() > mapMeshFirst_ + mapMeshCount_) {
        auto& mesh = meshes_.back();
        if (mesh.wireIndexBuffer) glapi::DeleteBuffers(1, &mesh.wireIndexBuffer);
        if (mesh.indexBuffer) glapi::DeleteBuffers(1, &mesh.indexBuffer);
        if (mesh.vertexBuffer) glapi::DeleteBuffers(1, &mesh.vertexBuffer);
        if (mesh.vao) glapi::DeleteVertexArrays(1, &mesh.vao);
        meshes_.pop_back();
    }
    worldActorMeshFirst_ = worldActorMeshCount_ = actorMeshFirst_ = actorMeshCount_ = 0;

    const std::vector<GpuMesh> mapMeshes(meshes_.begin() + static_cast<std::ptrdiff_t>(mapMeshFirst_),
                                         meshes_.begin() + static_cast<std::ptrdiff_t>(mapMeshFirst_ + mapMeshCount_));
    const std::size_t oldMapMeshFirst = mapMeshFirst_;

    const std::size_t deleteCount = std::min(mainMeshCount_, meshes_.size());
    for (std::size_t i = 0; i < deleteCount; ++i) {
        auto& mesh = meshes_[i];
        if (mesh.wireIndexBuffer) glapi::DeleteBuffers(1, &mesh.wireIndexBuffer);
        if (mesh.indexBuffer) glapi::DeleteBuffers(1, &mesh.indexBuffer);
        if (mesh.vertexBuffer) glapi::DeleteBuffers(1, &mesh.vertexBuffer);
        if (mesh.vao) glapi::DeleteVertexArrays(1, &mesh.vao);
    }
    meshes_.clear();

    if (!appendSceneMeshes(scene, error)) {
        meshes_ = mapMeshes;
        mapMeshFirst_ = 0;
        mainMeshCount_ = 0;
        return false;
    }
    mainMeshCount_ = meshes_.size();
    classMainFirst_[0] = 0;
    classMainCount_[0] = mainMeshCount_;

    mapMeshFirst_ = meshes_.size();
    meshes_.insert(meshes_.end(), mapMeshes.begin(), mapMeshes.end());

    const std::ptrdiff_t delta = static_cast<std::ptrdiff_t>(mapMeshFirst_) - static_cast<std::ptrdiff_t>(oldMapMeshFirst);
    if (delta != 0) {
        auto shiftIndices = [delta](std::vector<std::size_t>& bucket) {
            for (auto& idx : bucket) {
                idx = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(idx) + delta);
            }
        };
        shiftIndices(mapOpaqueMeshes_);
        shiftIndices(mapDecalMeshes_);
        shiftIndices(mapTransparentMeshes_);
        shiftIndices(mapAdditiveMeshes_);
        shiftIndices(mapShadowCasterMeshes_);
    }

    worldActorMeshFirst_ = meshes_.size();
    worldActorMeshCount_ = 0;
    if (worldActorScene) {
        if (!appendSceneMeshes(*worldActorScene, error)) return false;
        worldActorMeshCount_ = meshes_.size() - worldActorMeshFirst_;
    }

    actorMeshFirst_ = meshes_.size();
    actorMeshCount_ = 0;
    if (botActorScene) {
        if (!appendSceneMeshes(*botActorScene, error)) return false;
        actorMeshCount_ = meshes_.size() - actorMeshFirst_;
    }
    return true;
}

bool StageRenderer::appendMainSceneMeshes(const scene::CastScene& scene,std::size_t firstSourceMesh,std::string& error){
    if(classScenesResident_){error="Incremental upload is unavailable while both class rigs are resident";return false;}
    while(meshes_.size()>mainMeshCount_){auto& mesh=meshes_.back();if(mesh.wireIndexBuffer)glapi::DeleteBuffers(1,&mesh.wireIndexBuffer);if(mesh.indexBuffer)glapi::DeleteBuffers(1,&mesh.indexBuffer);if(mesh.vertexBuffer)glapi::DeleteBuffers(1,&mesh.vertexBuffer);if(mesh.vao)glapi::DeleteVertexArrays(1,&mesh.vao);meshes_.pop_back();}
    if(!appendSceneMeshes(scene,error,firstSourceMesh))return false;mainMeshCount_=meshes_.size();classMainFirst_[0]=0;classMainCount_[0]=mainMeshCount_;return true;
}

bool StageRenderer::loadClassScenes(const scene::CastScene& primary,const scene::CastScene& secondary,const scene::CastScene* primaryWorld,const scene::CastScene* secondaryWorld,const scene::CastScene* mapScene,const scene::CastScene* botActorScene,std::string& error,const scene::CastScene* third,const scene::CastScene* thirdWorld){
    clearScene();classScenesResident_=true;activeClassSlot_=0;classMainFirst_[0]=meshes_.size();if(!appendSceneMeshes(primary,error))return false;classMainCount_[0]=meshes_.size()-classMainFirst_[0];classMainFirst_[1]=meshes_.size();if(!appendSceneMeshes(secondary,error))return false;classMainCount_[1]=meshes_.size()-classMainFirst_[1];classMainFirst_[2]=meshes_.size();if(third&&!appendSceneMeshes(*third,error))return false;classMainCount_[2]=meshes_.size()-classMainFirst_[2];mainMeshCount_=meshes_.size();
    mapMeshFirst_=meshes_.size();mapMeshCount_=0;if(mapScene){if(!appendSceneMeshes(*mapScene,error,0,false,true))return false;mapMeshCount_=meshes_.size()-mapMeshFirst_;}
    classWorldFirst_[0]=meshes_.size();if(primaryWorld&&!appendSceneMeshes(*primaryWorld,error))return false;classWorldCount_[0]=meshes_.size()-classWorldFirst_[0];classWorldFirst_[1]=meshes_.size();if(secondaryWorld&&!appendSceneMeshes(*secondaryWorld,error))return false;classWorldCount_[1]=meshes_.size()-classWorldFirst_[1];classWorldFirst_[2]=meshes_.size();if(thirdWorld&&!appendSceneMeshes(*thirdWorld,error))return false;classWorldCount_[2]=meshes_.size()-classWorldFirst_[2];worldActorMeshFirst_=classWorldFirst_[0];worldActorMeshCount_=classWorldCount_[0];
    actorMeshFirst_=meshes_.size();actorMeshCount_=0;if(botActorScene){if(!appendSceneMeshes(*botActorScene,error))return false;actorMeshCount_=meshes_.size()-actorMeshFirst_;}return true;
}

bool StageRenderer::replaceClassScenes(const scene::CastScene& primary, const scene::CastScene& secondary, const scene::CastScene* primaryWorld, const scene::CastScene* secondaryWorld, const scene::CastScene* botActorScene, std::string& error,const scene::CastScene* third,const scene::CastScene* thirdWorld) {
    if (mapMeshCount_ == 0) {
        return loadClassScenes(primary, secondary, primaryWorld, secondaryWorld, nullptr, botActorScene, error, third, thirdWorld);
    }
    while (meshes_.size() > mapMeshFirst_ + mapMeshCount_) {
        auto& mesh = meshes_.back();
        if (mesh.wireIndexBuffer) glapi::DeleteBuffers(1, &mesh.wireIndexBuffer);
        if (mesh.indexBuffer) glapi::DeleteBuffers(1, &mesh.indexBuffer);
        if (mesh.vertexBuffer) glapi::DeleteBuffers(1, &mesh.vertexBuffer);
        if (mesh.vao) glapi::DeleteVertexArrays(1, &mesh.vao);
        meshes_.pop_back();
    }
    worldActorMeshFirst_ = worldActorMeshCount_ = actorMeshFirst_ = actorMeshCount_ = 0;
    classWorldFirst_ = {}; classWorldCount_ = {}; classMainFirst_ = {}; classMainCount_ = {};

    const std::vector<GpuMesh> mapMeshes(meshes_.begin() + static_cast<std::ptrdiff_t>(mapMeshFirst_),
                                         meshes_.begin() + static_cast<std::ptrdiff_t>(mapMeshFirst_ + mapMeshCount_));
    const std::size_t oldMapMeshFirst = mapMeshFirst_;

    const std::size_t deleteCount = std::min(mapMeshFirst_, meshes_.size());
    for (std::size_t i = 0; i < deleteCount; ++i) {
        auto& mesh = meshes_[i];
        if (mesh.wireIndexBuffer) glapi::DeleteBuffers(1, &mesh.wireIndexBuffer);
        if (mesh.indexBuffer) glapi::DeleteBuffers(1, &mesh.indexBuffer);
        if (mesh.vertexBuffer) glapi::DeleteBuffers(1, &mesh.vertexBuffer);
        if (mesh.vao) glapi::DeleteVertexArrays(1, &mesh.vao);
    }
    meshes_.clear();

    classScenesResident_ = true;
    activeClassSlot_ = 0;

    classMainFirst_[0] = meshes_.size();
    if (!appendSceneMeshes(primary, error)) {
        meshes_ = mapMeshes;
        mapMeshFirst_ = 0;
        mainMeshCount_ = 0;
        classScenesResident_ = false;
        return false;
    }
    classMainCount_[0] = meshes_.size() - classMainFirst_[0];

    classMainFirst_[1] = meshes_.size();
    if (!appendSceneMeshes(secondary, error)) {
        meshes_ = mapMeshes;
        mapMeshFirst_ = 0;
        mainMeshCount_ = 0;
        classScenesResident_ = false;
        return false;
    }
    classMainCount_[1] = meshes_.size() - classMainFirst_[1];
    classMainFirst_[2] = meshes_.size();
    if (third && !appendSceneMeshes(*third, error)) {
        meshes_.insert(meshes_.end(), mapMeshes.begin(), mapMeshes.end());
        mapMeshFirst_ = meshes_.size() - mapMeshCount_;
        classScenesResident_ = false;
        return false;
    }
    classMainCount_[2] = meshes_.size() - classMainFirst_[2];
    mainMeshCount_ = meshes_.size();

    mapMeshFirst_ = meshes_.size();
    meshes_.insert(meshes_.end(), mapMeshes.begin(), mapMeshes.end());

    const std::ptrdiff_t delta = static_cast<std::ptrdiff_t>(mapMeshFirst_) - static_cast<std::ptrdiff_t>(oldMapMeshFirst);
    if (delta != 0) {
        auto shiftIndices = [delta](std::vector<std::size_t>& bucket) {
            for (auto& idx : bucket) {
                idx = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(idx) + delta);
            }
        };
        shiftIndices(mapOpaqueMeshes_);
        shiftIndices(mapDecalMeshes_);
        shiftIndices(mapTransparentMeshes_);
        shiftIndices(mapAdditiveMeshes_);
        shiftIndices(mapShadowCasterMeshes_);
    }

    classWorldFirst_[0] = meshes_.size();
    if (primaryWorld && !appendSceneMeshes(*primaryWorld, error)) return false;
    classWorldCount_[0] = meshes_.size() - classWorldFirst_[0];

    classWorldFirst_[1] = meshes_.size();
    if (secondaryWorld && !appendSceneMeshes(*secondaryWorld, error)) return false;
    classWorldCount_[1] = meshes_.size() - classWorldFirst_[1];

    classWorldFirst_[2] = meshes_.size();
    if (thirdWorld && !appendSceneMeshes(*thirdWorld, error)) return false;
    classWorldCount_[2] = meshes_.size() - classWorldFirst_[2];

    worldActorMeshFirst_ = classWorldFirst_[0];
    worldActorMeshCount_ = classWorldCount_[0];

    actorMeshFirst_ = meshes_.size();
    actorMeshCount_ = 0;
    if (botActorScene) {
        if (!appendSceneMeshes(*botActorScene, error)) return false;
        actorMeshCount_ = meshes_.size() - actorMeshFirst_;
    }
    return true;
}

bool StageRenderer::loadActorScene(const scene::CastScene& scene,std::string& error){
    return loadAuxiliaryScenes(nullptr,nullptr,&scene,error);
}

bool StageRenderer::replaceBotScene(const scene::CastScene* scene,std::string& error){
    overlayHistory_.actors.clear();
    // Bot meshes form the final range. Keep both class slots, world actors,
    // map batches and their textures resident when only the bots change.
    if(actorMeshCount_&&actorMeshFirst_+actorMeshCount_!=meshes_.size()){
        error="Bot mesh range is not the final resident range";return false;
    }
    const auto first=actorMeshCount_?actorMeshFirst_:meshes_.size();
    const auto truncate=[&]{while(meshes_.size()>first){
        auto& mesh=meshes_.back();
        if(mesh.wireIndexBuffer)glapi::DeleteBuffers(1,&mesh.wireIndexBuffer);
        if(mesh.indexBuffer)glapi::DeleteBuffers(1,&mesh.indexBuffer);
        if(mesh.vertexBuffer)glapi::DeleteBuffers(1,&mesh.vertexBuffer);
        if(mesh.vao)glapi::DeleteVertexArrays(1,&mesh.vao);
        meshes_.pop_back();
    }};
    truncate();actorMeshFirst_=first;actorMeshCount_=0;
    if(scene&&!appendSceneMeshes(*scene,error)){truncate();return false;}
    actorMeshCount_=meshes_.size()-first;return true;
}

bool StageRenderer::loadAuxiliaryScenes(const scene::CastScene* mapScene,const scene::CastScene* worldActorScene,const scene::CastScene* botActorScene,std::string& error,ProgressCallback progressCallback){
    classScenesResident_=false;
    while(meshes_.size()>mainMeshCount_){auto& mesh=meshes_.back();if(mesh.wireIndexBuffer)glapi::DeleteBuffers(1,&mesh.wireIndexBuffer);if(mesh.indexBuffer)glapi::DeleteBuffers(1,&mesh.indexBuffer);if(mesh.vertexBuffer)glapi::DeleteBuffers(1,&mesh.vertexBuffer);if(mesh.vao)glapi::DeleteVertexArrays(1,&mesh.vao);meshes_.pop_back();}
    mapOpaqueMeshes_.clear();mapDecalMeshes_.clear();mapTransparentMeshes_.clear();mapAdditiveMeshes_.clear();mapShadowCasterMeshes_.clear();
    mapMeshFirst_=meshes_.size();mapMeshCount_=0;if(mapScene){if(!appendSceneMeshes(*mapScene,error,0,false,true,progressCallback))return false;mapMeshCount_=meshes_.size()-mapMeshFirst_;}
    worldActorMeshFirst_=meshes_.size();worldActorMeshCount_=0;if(worldActorScene){if(!appendSceneMeshes(*worldActorScene,error))return false;worldActorMeshCount_=meshes_.size()-worldActorMeshFirst_;}
    actorMeshFirst_=meshes_.size();actorMeshCount_=0;if(botActorScene){if(!appendSceneMeshes(*botActorScene,error))return false;actorMeshCount_=meshes_.size()-actorMeshFirst_;}return true;
}

bool StageRenderer::resizeTarget(int width,int height) {
    width=std::max(width,1);height=std::max(height,1);const bool stencil=actorOverlays_.outlines||actorOverlays_.glow||(actorOverlays_.chams&&actorOverlays_.throughWalls);if(width==width_&&height==height_&&stencil==overlayTargetStencil_)return true;releaseTarget();overlayTargetStencil_=stencil;
    width_=width;height_=height;glapi::GenFramebuffers(1,&framebuffer_);glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);
    glGenTextures(1,&colorTexture_);glBindTexture(GL_TEXTURE_2D,colorTexture_);
    glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba16f,width_,height_,0,GL_RGBA,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,colorTexture_,0);
    glGenTextures(1,&depthBuffer_);glBindTexture(GL_TEXTURE_2D,depthBuffer_);glTexImage2D(GL_TEXTURE_2D,0,stencil?0x8CAD:glapi::DepthComponent32f,width_,height_,0,stencil?0x84F9:GL_DEPTH_COMPONENT,stencil?0x8DAD:GL_FLOAT,nullptr); 
    // DEPTH32F_STENCIL8 preserves float depth precision.
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);glapi::FramebufferTexture2D(glapi::Framebuffer,stencil?0x821A:glapi::DepthAttachment,GL_TEXTURE_2D,depthBuffer_,0);
    glapi::GenFramebuffers(1,&postFramebuffer_);glapi::BindFramebuffer(glapi::Framebuffer,postFramebuffer_);glGenTextures(1,&postTexture_);glBindTexture(GL_TEXTURE_2D,postTexture_);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba16f,width_,height_,0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,postTexture_,0);glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);
    glapi::GenFramebuffers(1,&bloomFramebuffer_);for(std::size_t i=0;i<bloomTextures_.size();++i){bloomWidths_[i]=std::max(1,width_>>(static_cast<int>(i)+1));bloomHeights_[i]=std::max(1,height_>>(static_cast<int>(i)+1));glGenTextures(1,&bloomTextures_[i]);glBindTexture(GL_TEXTURE_2D,bloomTextures_[i]);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba16f,bloomWidths_[i],bloomHeights_[i],0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);}
    const bool complete=glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete;
    glapi::BindFramebuffer(glapi::Framebuffer,0);return complete;
}

bool StageRenderer::resizeMsaaTarget(int width,int height){
    width=std::max(width,1);height=std::max(height,1);const bool stencil=actorOverlays_.outlines||actorOverlays_.glow||(actorOverlays_.chams&&actorOverlays_.throughWalls);if(msaaFramebuffer_&&width==msaaWidth_&&height==msaaHeight_&&stencil==overlayMsaaStencil_)return true;releaseMsaaTarget();overlayMsaaStencil_=stencil;msaaWidth_=width;msaaHeight_=height;
    glapi::GenFramebuffers(1,&msaaFramebuffer_);glapi::BindFramebuffer(glapi::Framebuffer,msaaFramebuffer_);
    glapi::GenRenderbuffers(1,&msaaColorBuffer_);glapi::BindRenderbuffer(glapi::Renderbuffer,msaaColorBuffer_);glapi::RenderbufferStorageMultisample(glapi::Renderbuffer,4,glapi::Rgba16f,width,height);glapi::FramebufferRenderbuffer(glapi::Framebuffer,glapi::ColorAttachment0,glapi::Renderbuffer,msaaColorBuffer_);
    glapi::GenRenderbuffers(1,&msaaDepthBuffer_);glapi::BindRenderbuffer(glapi::Renderbuffer,msaaDepthBuffer_);glapi::RenderbufferStorageMultisample(glapi::Renderbuffer,4,stencil?0x8CAD:glapi::DepthComponent32f,width,height);glapi::FramebufferRenderbuffer(glapi::Framebuffer,stencil?0x821A:glapi::DepthAttachment,glapi::Renderbuffer,msaaDepthBuffer_);
    const bool complete=glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete;glapi::BindFramebuffer(glapi::Framebuffer,0);if(!complete)releaseMsaaTarget();return complete;
}

bool StageRenderer::resizeShadowTarget(int resolution){
    resolution=std::clamp(resolution,256,16384);if(shadowTexture_&&shadowResolution_==resolution)return true;
    if(shadowTexture_)glDeleteTextures(1,&shadowTexture_);shadowTexture_=0;shadowResolution_=0;
    glGenTextures(1,&shadowTexture_);glBindTexture(GL_TEXTURE_2D,shadowTexture_);glTexImage2D(GL_TEXTURE_2D,0,glapi::DepthComponent24,resolution,resolution,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToBorder);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToBorder);const float border[]{1,1,1,1};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);glapi::BindFramebuffer(glapi::Framebuffer,shadowFramebuffer_);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::DepthAttachment,GL_TEXTURE_2D,shadowTexture_,0);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);const bool complete=glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete;glapi::BindFramebuffer(glapi::Framebuffer,0);if(complete)shadowResolution_=resolution;return complete;
}

bool StageRenderer::resizeViewmodelShadowTarget(int resolution){
    resolution=std::clamp(resolution,256,16384);if(viewmodelShadowTexture_&&viewmodelShadowResolution_==resolution)return true;
    if(viewmodelShadowTexture_)glDeleteTextures(1,&viewmodelShadowTexture_);viewmodelShadowTexture_=0;viewmodelShadowResolution_=0;
    glGenTextures(1,&viewmodelShadowTexture_);glBindTexture(GL_TEXTURE_2D,viewmodelShadowTexture_);glTexImage2D(GL_TEXTURE_2D,0,glapi::DepthComponent24,resolution,resolution,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToBorder);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToBorder);const float border[]{1,1,1,1};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);glapi::BindFramebuffer(glapi::Framebuffer,viewmodelShadowFramebuffer_);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::DepthAttachment,GL_TEXTURE_2D,viewmodelShadowTexture_,0);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);const bool complete=glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete;glapi::BindFramebuffer(glapi::Framebuffer,0);if(complete)viewmodelShadowResolution_=resolution;return complete;
}

bool StageRenderer::resizeFarShadowTarget(int resolution){
    resolution=std::clamp(resolution,256,16384);if(farShadowTexture_&&farShadowResolution_==resolution)return true;
    if(farShadowTexture_)glDeleteTextures(1,&farShadowTexture_);farShadowTexture_=0;farShadowResolution_=0;
    glGenTextures(1,&farShadowTexture_);glBindTexture(GL_TEXTURE_2D,farShadowTexture_);glTexImage2D(GL_TEXTURE_2D,0,glapi::DepthComponent24,resolution,resolution,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToBorder);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToBorder);const float border[]{1,1,1,1};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);glapi::BindFramebuffer(glapi::Framebuffer,farShadowFramebuffer_);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::DepthAttachment,GL_TEXTURE_2D,farShadowTexture_,0);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);const bool complete=glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete;glapi::BindFramebuffer(glapi::Framebuffer,0);if(complete)farShadowResolution_=resolution;return complete;
}

void StageRenderer::drawLines(const std::vector<float>& vertices,const scene::Mat4& viewProjection) {
    if(vertices.empty())return;glapi::UseProgram(lineProgram_);
    glapi::UniformMatrix4fv(uniformLocation(lineProgram_,"uViewProjection"),1,GL_FALSE,viewProjection.data());
    glapi::BindVertexArray(lineVao_);glapi::BindBuffer(glapi::ArrayBuffer,lineBuffer_);
    glapi::BufferData(glapi::ArrayBuffer,static_cast<glapi::Size>(vertices.size()*sizeof(float)),vertices.data(),glapi::DynamicDraw);
    glapi::DrawArrays(GL_LINES,0,static_cast<GLsizei>(vertices.size()/7));
}

void StageRenderer::drawTriangles(const std::vector<float>& vertices,const scene::Mat4& viewProjection){if(vertices.empty())return;glapi::UseProgram(lineProgram_);glapi::UniformMatrix4fv(uniformLocation(lineProgram_,"uViewProjection"),1,GL_FALSE,viewProjection.data());glapi::BindVertexArray(lineVao_);glapi::BindBuffer(glapi::ArrayBuffer,lineBuffer_);glapi::BufferData(glapi::ArrayBuffer,static_cast<glapi::Size>(vertices.size()*sizeof(float)),vertices.data(),glapi::DynamicDraw);glapi::DrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices.size()/7));}

void StageRenderer::setShadowMaterial(const GpuMesh& mesh){
    const bool masked=mesh.materialPolicyExplicit&&mesh.alphaTest;
    glapi::Uniform1i(uniformLocation(shadowProgram_,"uShadowMask"),masked?1:0);
    if(!masked)return;
    glapi::Uniform1i(uniformLocation(shadowProgram_,"uShadowAlbedo"),1);
    glapi::Uniform1i(uniformLocation(shadowProgram_,"uShadowHasAlbedo"),mesh.texture?1:0);
    glapi::Uniform1i(uniformLocation(shadowProgram_,"uShadowVertexAlpha"),mesh.useVertexColor?1:0);
    glapi::Uniform1f(uniformLocation(shadowProgram_,"uShadowCutoff"),mesh.alphaCutoff);
    glapi::Uniform1f(uniformLocation(shadowProgram_,"uShadowColorAlpha"),mesh.color.w);
    glapi::ActiveTexture(glapi::Texture0+1);glBindTexture(GL_TEXTURE_2D,mesh.texture);
}

void StageRenderer::render(const scene::CastScene& scene,const std::vector<scene::Mat4>& globalPose,
                           const scene::Mat4& viewProjection,int width,int height,bool showGrid,bool showSkeleton,bool wireframe,
                           const scene::CastScene* actorScene,const std::vector<std::vector<scene::Mat4>>* actorPoses,const std::vector<int>* actorVariants,
                           const scene::CastScene* worldActorScene,const std::vector<scene::Mat4>* worldActorPose,bool showMainScene,
                           const scene::CastScene* mapScene,const gameplay::bot::NavigationGraph* navigation,
                           const std::vector<scene::Vec3>* botSpawns,int selectedNavigationNode,int selectedNavigationBlock,bool navigationOnTop,
                           const std::vector<take::DollyCameraKeyframe>* campathOverlay,int selectedCampathNode,
                           const take::DollyCameraKeyframe* activeCameraSample) {
    if(!initialized_||!resizeTarget(width,height)||(msaaEnabled_&&!resizeMsaaTarget(width,height)))return;
    foregroundDrawn_=showMainScene;
    GpuTimer::Scope gpuTiming(gpuTimer_);
    lastVisibleMapMeshes_=lastShadowCasterDraws_=lastFarShadowCasterDraws_=lastTotalDrawCalls_=0;
    const auto equipmentHidden=[&](const GpuMesh& mesh,const scene::CastScene& drawScene,const std::vector<scene::Mat4>& drawPose){
        if(&drawScene==&scene)return equipmentHiddenPlayer_&&mesh.viewmodelWeapon;
        // Player/bot world attachments are the carried weapon meshes; body
        // bones are never scaled or hidden, so hands and pose remain intact.
        if(mesh.attachmentIndex<0)return false;
        if(&drawScene==worldActorScene)return equipmentHiddenPlayer_;
        if(&drawScene==actorScene&&actorPoses)for(std::size_t i=0;i<equipmentHiddenActors_.size()&&i<actorPoses->size();++i)if(equipmentHiddenActors_[i] && &(*actorPoses)[i]==&drawPose)return true;
        return false;
    };
    const auto uploadUncachedPose=[&](const scene::CastScene& drawScene,const std::vector<scene::Mat4>& drawPose){std::vector<scene::Mat4> skin;if(drawPose.empty())skin.push_back(scene::Mat4::identity());else{skin.reserve(drawPose.size());for(std::size_t i=0;i<drawPose.size()&&i<drawScene.skeleton.bones.size();++i)skin.push_back(drawPose[i]*drawScene.skeleton.bones[i].inverseBind);if(skin.empty())skin.push_back(scene::Mat4::identity());}glapi::BindBuffer(glapi::TextureBuffer,boneBuffer_);glapi::BufferData(glapi::TextureBuffer,static_cast<glapi::Size>(skin.size()*sizeof(scene::Mat4)),skin.data(),glapi::DynamicDraw);glapi::ActiveTexture(glapi::Texture0+16);glBindTexture(glapi::TextureBuffer,boneTexture_);glapi::TexBuffer(glapi::TextureBuffer,glapi::Rgba32f,boneBuffer_);std::vector<float> shown(drawPose.empty()?1:drawPose.size(),1.0f);for(std::size_t i=0;i<shown.size()&&i<drawScene.skeleton.bones.size();++i){std::int32_t bone=static_cast<std::int32_t>(i);while(bone>=0){if(drawScene.hiddenBones.contains(static_cast<std::size_t>(bone))){shown[i]=0;break;}bone=drawScene.skeleton.bones[static_cast<std::size_t>(bone)].parent;}}glapi::BindBuffer(glapi::TextureBuffer,visibilityBuffer_);glapi::BufferData(glapi::TextureBuffer,static_cast<glapi::Size>(shown.size()*sizeof(float)),shown.data(),glapi::DynamicDraw);glapi::ActiveTexture(glapi::Texture0+17);glBindTexture(glapi::TextureBuffer,visibilityTexture_);glapi::TexBuffer(glapi::TextureBuffer,glapi::R32f,visibilityBuffer_);};
    std::size_t usedPoses=0;
    lastPoseRequests_=lastPoseUploads_=0;
    const auto uploadPose=[&](const scene::CastScene& drawScene,const std::vector<scene::Mat4>& drawPose){
        ++lastPoseRequests_;
        if(!poseUploadCacheEnabled_){++lastPoseUploads_;uploadUncachedPose(drawScene,drawPose);return;}
        std::size_t index=0;
        for(;index<usedPoses;++index)if(poseUploads_[index].scene==&drawScene&&poseUploads_[index].pose==&drawPose)break;
        if(index==usedPoses){
            if(index==poseUploads_.size())poseUploads_.emplace_back();
            auto& p=poseUploads_[index];p.scene=&drawScene;p.pose=&drawPose;++usedPoses;++lastPoseUploads_;
            p.skin.clear();p.skin.reserve(drawPose.size());
            for(std::size_t i=0;i<std::min(drawPose.size(),drawScene.skeleton.bones.size());++i)p.skin.push_back(drawPose[i]*drawScene.skeleton.bones[i].inverseBind);
            if(p.skin.empty())p.skin.push_back(scene::Mat4::identity());
            const bool visibilityChanged=p.visibilityState.update(drawScene.skeleton,drawScene.hiddenBones,drawPose.empty()?1:drawPose.size(),p.shown);
            const bool newVisibilityBuffer=!p.visibility;
            if(!p.bones){glapi::GenBuffers(1,&p.bones);glGenTextures(1,&p.boneTexture);glapi::GenBuffers(1,&p.visibility);glGenTextures(1,&p.visibilityTexture);}
            glapi::BindBuffer(glapi::TextureBuffer,p.bones);
            glapi::BufferData(glapi::TextureBuffer,static_cast<glapi::Size>(p.skin.size()*sizeof(scene::Mat4)),p.skin.data(),glapi::DynamicDraw);
            glapi::ActiveTexture(glapi::Texture0+16);glBindTexture(glapi::TextureBuffer,p.boneTexture);glapi::TexBuffer(glapi::TextureBuffer,glapi::Rgba32f,p.bones);
            if(visibilityChanged||newVisibilityBuffer){
                glapi::BindBuffer(glapi::TextureBuffer,p.visibility);
                glapi::BufferData(glapi::TextureBuffer,static_cast<glapi::Size>(p.shown.size()*sizeof(float)),p.shown.data(),glapi::DynamicDraw);
            }
            glapi::ActiveTexture(glapi::Texture0+17);glBindTexture(glapi::TextureBuffer,p.visibilityTexture);glapi::TexBuffer(glapi::TextureBuffer,glapi::R32f,p.visibility);
        }else{
            const auto& p=poseUploads_[index];
            glapi::ActiveTexture(glapi::Texture0+16);glBindTexture(glapi::TextureBuffer,p.boneTexture);
            glapi::ActiveTexture(glapi::Texture0+17);glBindTexture(glapi::TextureBuffer,p.visibilityTexture);
        }
    };
    uploadPose(scene,globalPose);
    const std::size_t activeMainFirst=classScenesResident_?classMainFirst_[activeClassSlot_]:0,activeMainCount=classScenesResident_?classMainCount_[activeClassSlot_]:mainMeshCount_,activeWorldFirst=classScenesResident_?classWorldFirst_[activeClassSlot_]:worldActorMeshFirst_,activeWorldCount=classScenesResident_?classWorldCount_[activeClassSlot_]:worldActorMeshCount_;
    std::size_t usedBounds=0;
    const auto actorBounds=[&](const scene::CastScene& rig,const std::vector<scene::Mat4>& pose,std::size_t first,std::size_t count,int variant,bool cache){
        if(cache)for(std::size_t i=0;i<usedBounds;++i){const auto& e=actorBoundsScratch_[i];if(e.scene==&rig&&e.pose==&pose&&e.first==first&&e.count==count&&e.variant==variant)return e.bounds;}
        scene::Bounds bounds;
        if(first<=meshes_.size()&&count<=meshes_.size()-first)for(std::size_t i=first;i<first+count;++i){
            const auto& mesh=meshes_[i];if(variant>=0&&mesh.actorVariant>=0&&mesh.actorVariant!=variant)continue;
            auto model=mesh.model;
            if(mesh.attachmentIndex>=0&&static_cast<std::size_t>(mesh.attachmentIndex)<rig.attachments.size()){
                const auto& a=rig.attachments[mesh.attachmentIndex];
                if(a.boneIndex<pose.size())model=pose[a.boneIndex]*a.localMatrix()*model;
            }
            if(!mesh.cullBounds.append(bounds,rig.skeleton,pose,model,mesh.skinned)){bounds={};break;}
        }
        if(!cache)return bounds;
        if(usedBounds==actorBoundsScratch_.size())actorBoundsScratch_.emplace_back();
        actorBoundsScratch_[usedBounds++]={&rig,&pose,first,count,variant,bounds};return bounds;
    };
    const auto cameraFrustum=visibility::paddedFrustum(viewProjection,2.f,width,height);
    const auto actorVisible=[&](const scene::CastScene& rig,const std::vector<scene::Mat4>& pose,std::size_t first,std::size_t count,int variant,const scene::Frustum& frustum,bool cache=true){
        return !actorCullingEnabled_||visibility::visible(actorBounds(rig,pose,first,count,variant,cache),frustum);
    };
    if (sunEnabled_ && sunShadows_){resizeShadowTarget(requestedShadowResolution_);if(viewmodelSelfShadows_)resizeViewmodelShadowTarget(requestedViewmodelShadowResolution_);if(farShadowEnabled_)resizeFarShadowTarget(requestedFarShadowResolution_);}
    scene::Bounds lightBounds =
        mapScene && mapScene->bounds.valid ? mapScene->bounds : scene.bounds;
    scene::Mat4 lightViewProjection = scene::Mat4::identity();
    scene::Mat4 farLightViewProjection = scene::Mat4::identity();
    scene::Mat4 viewmodelLightViewProjection = scene::Mat4::identity();
    if (sunEnabled_ && lightBounds.valid) {
      const scene::Vec3 center = shadowFocus_;
      const float radius = std::max(400.0f, shadowDistance_);
      auto direction = scene::normalize(sunDirection_);
      if (scene::length(direction) < 0.5f)
        direction = {-0.45f, -0.35f, -0.82f};
      const scene::Vec3 up = std::abs(direction.z) > 0.95f
                                 ? scene::Vec3{0, 1, 0}
                                 : scene::Vec3{0, 0, 1};
      const auto lightView = scene::lookAt(center - direction * radius * 1.8f,
                                           center, up),
                 lightProjection = scene::orthographic(
                     -radius, radius, -radius, radius, 1.0f, radius * 4.0f);
      lightViewProjection = lightProjection * lightView;
      const float farRadius=std::max(radius,farShadowDistance_);const auto farView=scene::lookAt(center-direction*farRadius*1.8f,center,up),farProjection=scene::orthographic(-farRadius,farRadius,-farRadius,farRadius,1.0f,farRadius*4.0f);farLightViewProjection=farProjection*farView;
      const float vmRadius=45.0f;
      const auto vmLightView=scene::lookAt(center-direction*vmRadius*2.0f,center,up),vmLightProjection=scene::orthographic(-vmRadius,vmRadius,-vmRadius,vmRadius,1.0f,vmRadius*4.0f);
      viewmodelLightViewProjection=vmLightProjection*vmLightView;
    }
    if (sunEnabled_ && sunShadows_ && shadowFramebuffer_ && shadowTexture_ &&
        lightBounds.valid) {
      glapi::BindFramebuffer(glapi::Framebuffer, shadowFramebuffer_);
      glViewport(0, 0, shadowResolution_, shadowResolution_);
      glClear(GL_DEPTH_BUFFER_BIT);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(1.5f, 3.0f);
      glapi::UseProgram(shadowProgram_);
      glapi::UniformMatrix4fv(
          uniformLocation(shadowProgram_, "uLightViewProjection"), 1,
          GL_FALSE, lightViewProjection.data());
      glapi::Uniform1i(uniformLocation(shadowProgram_, "uBones"), 16);
      glapi::Uniform1i(
          uniformLocation(shadowProgram_, "uBoneVisibility"), 17);      const auto drawDepth = [&](const GpuMesh &mesh,
                                 const scene::CastScene &drawScene,
                                 const std::vector<scene::Mat4> &drawPose) {
        if(mesh.eyeOverlay||equipmentHidden(mesh,drawScene,drawPose))return;
        auto model = mesh.model;
        if (mesh.attachmentIndex >= 0 &&
            static_cast<std::size_t>(mesh.attachmentIndex) <
                drawScene.attachments.size()) {
          const auto &attachment = drawScene.attachments[mesh.attachmentIndex];
          if (attachment.boneIndex < drawPose.size())
            model = drawPose[attachment.boneIndex] * attachment.localMatrix() *
                    model;
        }
        glapi::UniformMatrix4fv(shadowUniforms_.model, 1, GL_FALSE, model.data());
        glapi::Uniform1i(shadowUniforms_.skinned, mesh.skinned ? 1 : 0);
        setShadowMaterial(mesh);
        glapi::BindVertexArray(mesh.vao);
        glapi::BindBuffer(glapi::ElementArrayBuffer, mesh.indexBuffer);
        glapi::DrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT,
                            nullptr);
      };
      if (mapScene && !mapShadowCasterMeshes_.empty()) {
        const auto lightFrustum = scene::extractFrustum(lightViewProjection);
        uploadPose(*mapScene, {});
        glapi::UniformMatrix4fv(shadowUniforms_.model, 1, GL_FALSE, scene::Mat4::identity().data());
        glapi::Uniform1i(shadowUniforms_.skinned, 0);
        unsigned lastShadowVao = 0;
        for (const auto meshIdx : mapShadowCasterMeshes_) {
          if (meshIdx >= meshes_.size()) continue;
          const auto& mesh = meshes_[meshIdx];
          if (mesh.hasBounds && !scene::isAabbInFrustum(lightFrustum, mesh.aabbMin, mesh.aabbMax))
            continue;
          if (lastShadowVao != mesh.vao) {
            glapi::BindVertexArray(mesh.vao);
            lastShadowVao = mesh.vao;
          }
          setShadowMaterial(mesh);
          glapi::DrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
          lastShadowCasterDraws_++;
          lastTotalDrawCalls_++;
        }
      }
      const auto actorLightFrustum=scene::extractFrustum(lightViewProjection);
      if (worldActorScene && worldActorPose && actorVisible(*worldActorScene,*worldActorPose,activeWorldFirst,activeWorldCount,-1,actorLightFrustum)) {
        uploadPose(*worldActorScene, *worldActorPose);
        for (std::size_t i = activeWorldFirst;
             i < activeWorldFirst + activeWorldCount && i < meshes_.size(); ++i)
          drawDepth(meshes_[i], *worldActorScene, *worldActorPose);
      }
      if (actorScene && actorPoses)
        for (std::size_t actorIndex=0;actorIndex<actorPoses->size();++actorIndex) {
          const auto &actorPose=(*actorPoses)[actorIndex];
          const int variant=actorVariants&&actorIndex<actorVariants->size()?(*actorVariants)[actorIndex]:-1;
          if(!actorVisible(*actorScene,actorPose,actorMeshFirst_,actorMeshCount_,variant,actorLightFrustum))continue;
          uploadPose(*actorScene, actorPose);
          for (std::size_t i = actorMeshFirst_;
               i < actorMeshFirst_ + actorMeshCount_; ++i)
            if(variant<0||meshes_[i].actorVariant<0||meshes_[i].actorVariant==variant)drawDepth(meshes_[i], *actorScene, actorPose);
        }
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
    // Viewmodels receive the world/map shadow above, but cast into a separate
    // depth map with high-precision viewmodel projection tightly bounded to the hands and weapon.
    if(sunEnabled_&&sunShadows_&&viewmodelSelfShadows_&&showMainScene&&viewmodelShadowFramebuffer_&&viewmodelShadowTexture_&&lightBounds.valid){glapi::BindFramebuffer(glapi::Framebuffer,viewmodelShadowFramebuffer_);glViewport(0,0,viewmodelShadowResolution_,viewmodelShadowResolution_);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(0.5f,1.0f);glapi::UseProgram(shadowProgram_);glapi::UniformMatrix4fv(shadowUniforms_.lightViewProjection,1,GL_FALSE,viewmodelLightViewProjection.data());glapi::Uniform1i(shadowUniforms_.bones,16);glapi::Uniform1i(shadowUniforms_.boneVisibility,17);uploadPose(scene,globalPose);for(std::size_t i=activeMainFirst;i<std::min(activeMainFirst+activeMainCount,meshes_.size());++i){const auto& mesh=meshes_[i];if(equipmentHidden(mesh,scene,globalPose))continue;auto model=mesh.model;if(mesh.attachmentIndex>=0&&static_cast<std::size_t>(mesh.attachmentIndex)<scene.attachments.size()){const auto& attachment=scene.attachments[mesh.attachmentIndex];if(attachment.boneIndex<globalPose.size())model=globalPose[attachment.boneIndex]*attachment.localMatrix()*model;}glapi::UniformMatrix4fv(shadowUniforms_.model,1,GL_FALSE,model.data());glapi::Uniform1i(shadowUniforms_.skinned,mesh.skinned?1:0);glapi::BindVertexArray(mesh.vao);glapi::BindBuffer(glapi::ElementArrayBuffer,mesh.indexBuffer);setShadowMaterial(mesh);glapi::DrawElements(GL_TRIANGLES,mesh.indexCount,GL_UNSIGNED_INT,nullptr);}glDisable(GL_POLYGON_OFFSET_FILL);}
    if(sunEnabled_&&sunShadows_&&farShadowEnabled_&&farShadowFramebuffer_&&farShadowTexture_&&lightBounds.valid&&mapScene&&!mapShadowCasterMeshes_.empty()){
        glapi::BindFramebuffer(glapi::Framebuffer,farShadowFramebuffer_);
        glViewport(0,0,farShadowResolution_,farShadowResolution_);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f,3.0f);
        glapi::UseProgram(shadowProgram_);
        glapi::UniformMatrix4fv(shadowUniforms_.lightViewProjection,1,GL_FALSE,farLightViewProjection.data());
        glapi::Uniform1i(shadowUniforms_.bones,16);
        glapi::Uniform1i(shadowUniforms_.boneVisibility,17);
        glapi::UniformMatrix4fv(shadowUniforms_.model,1,GL_FALSE,scene::Mat4::identity().data());
        glapi::Uniform1i(shadowUniforms_.skinned,0);
        uploadPose(*mapScene,{});
        const auto farFrustum=scene::extractFrustum(farLightViewProjection);
        unsigned lastFarVao = 0;
        for(const auto meshIdx : mapShadowCasterMeshes_){
            if(meshIdx >= meshes_.size()) continue;
            const auto& mesh=meshes_[meshIdx];
            if(mesh.hasBounds&&!scene::isAabbInFrustum(farFrustum,mesh.aabbMin,mesh.aabbMax))
                continue;
            if(lastFarVao != mesh.vao){
                glapi::BindVertexArray(mesh.vao);
                lastFarVao = mesh.vao;
            }
            setShadowMaterial(mesh);
            glapi::DrawElements(GL_TRIANGLES,mesh.indexCount,GL_UNSIGNED_INT,nullptr);
            lastFarShadowCasterDraws_++;
            lastTotalDrawCalls_++;
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    uploadPose(scene,globalPose);

    glapi::BindFramebuffer(glapi::Framebuffer,msaaEnabled_?msaaFramebuffer_:framebuffer_);glViewport(0,0,width_,height_);
    glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);if(debugView_==7)glClearColor(1,1,1,1);else if(debugView_==8)glClearColor(0,1,0,1);else glClearColor(viewmodelCapture_?captureBackground_.x:0.035f,viewmodelCapture_?captureBackground_.y:0.043f,viewmodelCapture_?captureBackground_.z:0.058f,viewmodelCapture_?captureBackground_.w:1.0f);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(environmentProgram_){glapi::UseProgram(environmentProgram_);glapi::Uniform1i(uniformLocation(environmentProgram_,"uDebugView"),debugView_);glapi::Uniform1f(uniformLocation(environmentProgram_,"uSkyContrast"),environmentContrast_);glapi::Uniform1f(uniformLocation(environmentProgram_,"uSkyHighlightBoost"),environmentHighlightBoost_);glapi::Uniform1f(uniformLocation(environmentProgram_,"uSkyHighlightThreshold"),environmentHighlightThreshold_);glapi::Uniform1f(uniformLocation(environmentProgram_,"uSkyToneMap"),environmentToneMap_);}
    const auto setExtendedFilmUniforms=[&](unsigned program){glapi::Uniform3f(uniformLocation(program,"uFilmMidTint"),filmMidTint_.x,filmMidTint_.y,filmMidTint_.z);glapi::Uniform1i(uniformLocation(program,"uFilmMidTintEnabled"),filmMidTintEnabled_?1:0);glapi::Uniform1i(uniformLocation(program,"uBloomEnabled"),bloomEnabled_?1:0);glapi::Uniform1f(uniformLocation(program,"uBloomThreshold"),bloomThreshold_);glapi::Uniform1f(uniformLocation(program,"uBloomIntensity"),bloomIntensity_);glapi::Uniform1i(uniformLocation(program,"uVignetteEnabled"),vignetteEnabled_?1:0);glapi::Uniform1f(uniformLocation(program,"uVignetteIntensity"),vignetteIntensity_);glapi::Uniform1f(uniformLocation(program,"uVignetteRadius"),vignetteRadius_);glapi::Uniform1f(uniformLocation(program,"uVignetteSoftness"),vignetteSoftness_);glapi::Uniform2f(uniformLocation(program,"uViewportSize"),static_cast<float>(width_),static_cast<float>(height_));};
    if(!viewmodelCapture_&&(environmentMode_||(dayNight_.enabled&&dayNight_.sky))&&environmentProgram_){
        glapi::UseProgram(environmentProgram_);
        const auto dn=daynight::evaluate(dayNight_,dayNightTime_);
        glapi::Uniform1i(uniformLocation(environmentProgram_,"uDayNightSky"),dayNight_.enabled&&dayNight_.sky);
        glapi::Uniform3f(uniformLocation(environmentProgram_,"uDnSun"),dn.sunDirection.x,dn.sunDirection.y,dn.sunDirection.z);
        glapi::Uniform3f(uniformLocation(environmentProgram_,"uDnSunset"),dayNight_.sunsetColor.x,dayNight_.sunsetColor.y,dayNight_.sunsetColor.z);
        glapi::Uniform4f(uniformLocation(environmentProgram_,"uDnAtmosphere"),dayNight_.rayleigh,dayNight_.mie,dayNight_.mieDirection,dayNight_.skyIntensity);
        glapi::Uniform4f(uniformLocation(environmentProgram_,"uDnCloud"),dayNight_.coverage,dayNight_.thickness,dayNight_.absorption,dayNight_.cloudScale);
        const double dnTime=dayNight_.running?dayNightTime_:0;
        const float wind=dayNight_.windAngle*scene::kPi/180.f;
        glapi::Uniform4f(uniformLocation(environmentProgram_,"uDnMotion"),float(std::cos(wind)*dnTime*dayNight_.windSpeed),float(std::sin(wind)*dnTime*dayNight_.windSpeed),dayNight_.cloudExposure,dayNight_.starIntensity);
        glapi::Uniform1f(uniformLocation(environmentProgram_,"uDnDisk"),dayNight_.sunDisk);
        glapi::Uniform4f(uniformLocation(environmentProgram_,"uDnStarTrail"),dayNight_.starTrailLength*scene::kPi/180.f,dayNight_.starTrailBrightness,dayNight_.starTrailFade,daynight::starAngleAt(dayNight_,dayNightTime_));
        const float poleAz=dayNight_.starPoleAzimuth*scene::kPi/180.f,poleEl=dayNight_.starPoleElevation*scene::kPi/180.f;
        glapi::Uniform3f(uniformLocation(environmentProgram_,"uDnStarPole"),std::cos(poleAz)*std::cos(poleEl),std::sin(poleAz)*std::cos(poleEl),std::sin(poleEl));
        glapi::Uniform1i(uniformLocation(environmentProgram_,"uDnSteps"),dayNight_.cloudSteps);
        glapi::Uniform1i(uniformLocation(environmentProgram_,"uDnLightSteps"),dayNight_.lightSteps);
        glapi::Uniform1i(uniformLocation(environmentProgram_,"uNightPhoto"),nightSky_.enabled);
        glapi::Uniform4f(uniformLocation(environmentProgram_,"uNightLook"),nightSky_.exposure,nightSky_.shadowLift,nightSky_.saturation,nightSky_.cloudLight);
        glapi::Uniform3f(uniformLocation(environmentProgram_,"uNightZenith"),nightSky_.zenithR,nightSky_.zenithG,nightSky_.zenithB);
        glapi::Uniform4f(uniformLocation(environmentProgram_,"uNightHorizon"),nightSky_.horizonR,nightSky_.horizonG,nightSky_.horizonB,nightSky_.horizonGlow);
        glapi::Uniform4f(uniformLocation(environmentProgram_,"uNightStars"),nightSky_.starDensity,nightSky_.starSize,nightSky_.starColor,nightSky_.starGlow);
        glapi::Uniform4f(uniformLocation(environmentProgram_,"uNightGalaxyNoise"),float(nightSky_.galaxyNoiseAlgorithm),nightSky_.galaxyNoiseScale,nightSky_.galaxyNoiseDetail,nightSky_.galaxyNoiseContrast);
        glapi::ActiveTexture(glapi::Texture0+4);glBindTexture(GL_TEXTURE_2D,nightPanorama_);
        glapi::Uniform1i(uniformLocation(environmentProgram_,"uNightPanorama"),4);
        glapi::Uniform1i(uniformLocation(environmentProgram_,"uNightPanoramaReady"),nightPanorama_!=0);
        glapi::Uniform4f(uniformLocation(environmentProgram_,"uNightGalaxy"),nightSky_.milkyWay,nightSky_.galaxyWidth*scene::kPi/180,nightSky_.galaxyTilt*scene::kPi/180,nightSky_.galaxyAzimuth*scene::kPi/180);
        const float moonAz=nightSky_.moonAzimuth*scene::kPi/180,moonEl=nightSky_.moonElevation*scene::kPi/180;
        glapi::Uniform3f(uniformLocation(environmentProgram_,"uNightMoonDir"),std::cos(moonAz)*std::cos(moonEl),std::sin(moonAz)*std::cos(moonEl),std::sin(moonEl));
        glapi::Uniform4f(uniformLocation(environmentProgram_,"uNightMoon"),nightSky_.moonIntensity,nightSky_.moonSize*scene::kPi/360,nightSky_.moonPhase,nightSky_.moonHalo);
glDisable(GL_DEPTH_TEST);glapi::UseProgram(environmentProgram_);const auto forward=scene::normalize(environmentForward_),right=scene::normalize(scene::cross(forward,environmentUp_)),up=scene::normalize(scene::cross(right,forward));glapi::Uniform1i(uniformLocation(environmentProgram_,"uMode"),environmentMode_);glapi::Uniform1i(uniformLocation(environmentProgram_,"uFlipVertical"),skyVerticalFlip_?1:0);glapi::Uniform3f(uniformLocation(environmentProgram_,"uForward"),forward.x,forward.y,forward.z);glapi::Uniform3f(uniformLocation(environmentProgram_,"uRightAxis"),right.x,right.y,right.z);glapi::Uniform3f(uniformLocation(environmentProgram_,"uUpAxis"),up.x,up.y,up.z);glapi::Uniform1f(uniformLocation(environmentProgram_,"uTanHalfFov"),std::tan(std::clamp(environmentFov_,1.0f,179.0f)*scene::kPi/360.0f));glapi::Uniform1f(uniformLocation(environmentProgram_,"uAspect"),std::max(0.01f,environmentAspect_));glapi::Uniform1f(uniformLocation(environmentProgram_,"uRotation"),environmentRotation_*scene::kPi/180.0f);glapi::Uniform1f(uniformLocation(environmentProgram_,"uIntensity"),environmentIntensity_);glapi::Uniform1f(uniformLocation(environmentProgram_,"uExposure"),environmentExposure_);glapi::Uniform1i(uniformLocation(environmentProgram_,"uFilmEnabled"),filmEnabled_?1:0);glapi::Uniform1f(uniformLocation(environmentProgram_,"uFilmBrightness"),filmBrightness_);glapi::Uniform1f(uniformLocation(environmentProgram_,"uFilmContrast"),filmContrast_);glapi::Uniform1f(uniformLocation(environmentProgram_,"uFilmDesaturation"),filmDesaturation_);glapi::Uniform3f(uniformLocation(environmentProgram_,"uFilmDarkTint"),filmDarkTint_.x,filmDarkTint_.y,filmDarkTint_.z);glapi::Uniform3f(uniformLocation(environmentProgram_,"uFilmLightTint"),filmLightTint_.x,filmLightTint_.y,filmLightTint_.z);glapi::Uniform1i(uniformLocation(environmentProgram_,"uFilmInvert"),filmInvert_?1:0);glapi::Uniform3f(uniformLocation(environmentProgram_,"uFogColor"),fogColor_.x,fogColor_.y,fogColor_.z);glapi::Uniform1f(uniformLocation(environmentProgram_,"uFogSkyAmount"),fogEnabled_?fogSkyAmount_:0.0f);for(int i=0;i<6;++i){const auto sourceName="uFaceSource"+std::to_string(i),rotationName="uFaceRotation"+std::to_string(i);glapi::Uniform1i(uniformLocation(environmentProgram_,sourceName.c_str()),skyFaceSources_[i]);glapi::Uniform1i(uniformLocation(environmentProgram_,rotationName.c_str()),skyFaceQuarterTurns_[i]);}const char* names[]={"uPanorama","uFront","uBack","uLeft","uRight","uUp","uDown"};glapi::ActiveTexture(glapi::Texture0+5);glBindTexture(GL_TEXTURE_2D,environmentTexture_);glapi::Uniform1i(uniformLocation(environmentProgram_,names[0]),5);for(int i=0;i<6;++i){glapi::ActiveTexture(glapi::Texture0+6+i);glBindTexture(GL_TEXTURE_2D,environmentFaces_[i]);glapi::Uniform1i(uniformLocation(environmentProgram_,names[i+1]),6+i);}glapi::BindVertexArray(lineVao_);glapi::DrawArrays(GL_TRIANGLES,0,3);glEnable(GL_DEPTH_TEST);}
    glapi::UseProgram(meshProgram_);setExtendedFilmUniforms(meshProgram_);glapi::Uniform1i(uniformLocation(meshProgram_,"uBloomEnabled"),0);
    glapi::UniformMatrix4fv(uniformLocation(meshProgram_,"uViewProjection"),1,GL_FALSE,viewProjection.data());
    const int foregroundClipLoc=uniformLocation(meshProgram_,"uForegroundClip");
    glapi::Uniform2f(foregroundClipLoc,0,0);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uBones"),16);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uBoneVisibility"),17);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uAlbedo"),1);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uNormalMap"),5);
    glapi::Uniform1f(meshUniforms_.normalIntensity,normalMapIntensity_);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uSpecularMap"),6);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uMetalnessMap"),0);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uWeaponMetalnessOverride"),weaponMetalnessOverride_);
    glapi::Uniform4f(uniformLocation(meshProgram_,"uWeaponMetalnessDiffuse"),weaponMetalnessFromDiffuse_?1.f:0.f,weaponMetalnessBlack_,weaponMetalnessWhite_,weaponMetalnessGamma_);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uRoughnessMap"),3);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uSpecularImperfections"),13);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uCamo"),2);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uHasCamo"),camoTexture_?1:0);
    glapi::ActiveTexture(glapi::Texture0+13);glBindTexture(GL_TEXTURE_2D,specularImperfectionsTexture_);glapi::Uniform1f(uniformLocation(meshProgram_,"uImperfectionStrength"),imperfectionStrength_);glapi::Uniform1f(uniformLocation(meshProgram_,"uImperfectionScale"),imperfectionScale_);glapi::Uniform1f(uniformLocation(meshProgram_,"uImperfectionLow"),imperfectionLow_);glapi::Uniform1f(uniformLocation(meshProgram_,"uImperfectionHigh"),imperfectionHigh_);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uCamoStrength"),camoStrength_);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uCamoScale"),camoScale_);
    glapi::Uniform2f(uniformLocation(meshProgram_,"uCamoOffset"),camoOffset_.x,camoOffset_.y);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uCamoRotation"),camoRotation_*scene::kPi/180.0f);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uCamoAlphaLow"),camoAlphaLow_);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uCamoAlphaHigh"),camoAlphaHigh_);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uCamoInvert"),camoInvert_?1:0);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uCamoLumaMask"),camoLumaMask_?1:0);glapi::Uniform1f(uniformLocation(meshProgram_,"uCamoLumaLow"),camoLumaLow_);glapi::Uniform1f(uniformLocation(meshProgram_,"uCamoLumaHigh"),camoLumaHigh_);glapi::Uniform1f(uniformLocation(meshProgram_,"uCamoLumaGamma"),camoLumaGamma_);glapi::Uniform1f(uniformLocation(meshProgram_,"uCamoLumaContrast"),camoLumaContrast_);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uLensAlpha"),lensAlpha_);
    glapi::Uniform3f(uniformLocation(meshProgram_,"uLensTint"),lensTint_.x,lensTint_.y,lensTint_.z);glapi::Uniform1f(uniformLocation(meshProgram_,"uLensTintIntensity"),lensTintIntensity_);glapi::Uniform1f(uniformLocation(meshProgram_,"uLensCubemapIntensity"),lensCubemapIntensity_);glapi::Uniform3f(uniformLocation(meshProgram_,"uEmissiveTint"),emissiveTint_.x,emissiveTint_.y,emissiveTint_.z);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uSpecularIntensity"),specularIntensity_);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uSpecularSharpness"),specularSharpness_);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uSpecularIntensity2"),specularIntensity2_);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uSpecularSharpness2"),specularSharpness2_);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uLensSpecularIntensity"),lensSpecularIntensity_);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uCubemapSpecular"),cubemapSpecular_?1:0);glapi::Uniform1i(uniformLocation(meshProgram_,"uHasSpecCubemap"),environmentMode_>0?1:0);glapi::Uniform1i(uniformLocation(meshProgram_,"uSkyMode"),environmentMode_);glapi::Uniform1f(uniformLocation(meshProgram_,"uSkyIntensity"),environmentIntensity_);glapi::Uniform1f(uniformLocation(meshProgram_,"uSkyExposure"),environmentExposure_);glapi::Uniform1f(uniformLocation(meshProgram_,"uSkyContrast"),environmentContrast_);glapi::Uniform1f(uniformLocation(meshProgram_,"uSkyHighlightBoost"),environmentHighlightBoost_);glapi::Uniform1f(uniformLocation(meshProgram_,"uSkyHighlightThreshold"),environmentHighlightThreshold_);glapi::Uniform1f(uniformLocation(meshProgram_,"uSkyToneMap"),environmentToneMap_);glapi::Uniform1f(uniformLocation(meshProgram_,"uCubemapSpecularIntensity"),cubemapSpecularIntensity_);glapi::Uniform1f(uniformLocation(meshProgram_,"uCubemapBlur"),cubemapBlur_);glapi::Uniform1f(uniformLocation(meshProgram_,"uWeaponCubemapBlur"),weaponCubemapBlur_);glapi::Uniform1f(uniformLocation(meshProgram_,"uSkyRotation"),environmentRotation_*scene::kPi/180.0f);glapi::Uniform1f(uniformLocation(meshProgram_,"uNormalReflectionInfluence"),surfaceNormalReflectionInfluence_);glapi::Uniform1i(uniformLocation(meshProgram_,"uCubemapSamples"),cubemapKawaseSamples_);glapi::Uniform1i(uniformLocation(meshProgram_,"uAlphaOverlay"),alphaOverlay_?1:0);glapi::Uniform1f(uniformLocation(meshProgram_,"uAlphaOverlayIntensity"),alphaOverlayIntensity_);glapi::Uniform1i(uniformLocation(meshProgram_,"uDebugView"),debugView_);glapi::Uniform1i(uniformLocation(meshProgram_,"uShadingModel"),shadingModel_);glapi::Uniform1i(uniformLocation(meshProgram_,"uBrdfModel"),brdfModel_);glapi::Uniform1i(uniformLocation(meshProgram_,"uIw3DualLobe"),iw3DualLobe_?1:0);glapi::Uniform1f(uniformLocation(meshProgram_,"uAwRoughnessScale"),awRoughnessScale_);glapi::Uniform1f(uniformLocation(meshProgram_,"uAwRoughnessBias"),awRoughnessBias_);glapi::Uniform1f(uniformLocation(meshProgram_,"uAwMetalness"),awMetalness_);glapi::Uniform1f(uniformLocation(meshProgram_,"uAwSpecularLevel"),awSpecularLevel_);glapi::Uniform1f(uniformLocation(meshProgram_,"uAwDiffuseWrap"),awDiffuseWrap_);glapi::Uniform1f(uniformLocation(meshProgram_,"uAwClearcoat"),awClearcoat_);glapi::Uniform1f(uniformLocation(meshProgram_,"uAwClearcoatRoughness"),awClearcoatRoughness_);glapi::Uniform1f(uniformLocation(meshProgram_,"uAwEnvironmentIntensity"),awEnvironmentIntensity_);
    glapi::Uniform2f(uniformLocation(meshProgram_,"uCubemapSurfaceMultipliers"),viewmodelCubemapMultiplier_,worldCubemapMultiplier_);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uFilmEnabled"),filmEnabled_?1:0);glapi::Uniform1f(uniformLocation(meshProgram_,"uFilmBrightness"),filmBrightness_);glapi::Uniform1f(uniformLocation(meshProgram_,"uFilmContrast"),filmContrast_);glapi::Uniform1f(uniformLocation(meshProgram_,"uFilmDesaturation"),filmDesaturation_);glapi::Uniform3f(uniformLocation(meshProgram_,"uFilmDarkTint"),filmDarkTint_.x,filmDarkTint_.y,filmDarkTint_.z);glapi::Uniform3f(uniformLocation(meshProgram_,"uFilmLightTint"),filmLightTint_.x,filmLightTint_.y,filmLightTint_.z);glapi::Uniform1i(uniformLocation(meshProgram_,"uFilmInvert"),filmInvert_?1:0);glapi::Uniform1i(uniformLocation(meshProgram_,"uFogEnabled"),fogEnabled_?1:0);glapi::Uniform3f(uniformLocation(meshProgram_,"uFogColor"),fogColor_.x,fogColor_.y,fogColor_.z);glapi::Uniform1f(uniformLocation(meshProgram_,"uFogStart"),fogStart_);glapi::Uniform1f(uniformLocation(meshProgram_,"uFogHalfDistance"),fogHalfDistance_);glapi::Uniform1f(uniformLocation(meshProgram_,"uFogOpacity"),fogOpacity_);glapi::Uniform1i(uniformLocation(meshProgram_,"uFogHeightEnabled"),fogHeightEnabled_?1:0);glapi::Uniform1f(uniformLocation(meshProgram_,"uFogHeight"),fogHeight_);glapi::Uniform1f(uniformLocation(meshProgram_,"uFogHeightFalloff"),fogHeightFalloff_);
    glapi::Uniform1i(uniformLocation(meshProgram_,"uFilmEnabled"),0);glapi::Uniform1i(uniformLocation(meshProgram_,"uSkyFlipVertical"),skyVerticalFlip_?1:0);
    const char* specFaceNames[]={"uSpecFront","uSpecBack","uSpecLeft","uSpecRight","uSpecUp","uSpecDown"};for(int i=0;i<6;++i){const auto sourceName="uSpecFaceSource"+std::to_string(i),rotationName="uSpecFaceRotation"+std::to_string(i);glapi::Uniform1i(uniformLocation(meshProgram_,sourceName.c_str()),skyFaceSources_[i]);glapi::Uniform1i(uniformLocation(meshProgram_,rotationName.c_str()),skyFaceQuarterTurns_[i]);glapi::ActiveTexture(glapi::Texture0+7+i);glBindTexture(GL_TEXTURE_2D,i==0&&environmentMode_==1?environmentTexture_:environmentFaces_[i]);glapi::Uniform1i(uniformLocation(meshProgram_,specFaceNames[i]),7+i);}
    glapi::Uniform1i(uniformLocation(meshProgram_,"uIgnoreAlpha"),ignoreViewmodelTextureAlpha_?1:0);
    glapi::UniformMatrix4fv(uniformLocation(meshProgram_,"uLightViewProjection"),1,GL_FALSE,lightViewProjection.data());glapi::UniformMatrix4fv(uniformLocation(meshProgram_,"uFarLightViewProjection"),1,GL_FALSE,farLightViewProjection.data());glapi::Uniform1i(uniformLocation(meshProgram_,"uSunEnabled"),sunEnabled_?1:0);glapi::Uniform1i(uniformLocation(meshProgram_,"uShadowEnabled"),sunEnabled_&&sunShadows_?1:0);glapi::Uniform1i(uniformLocation(meshProgram_,"uFarShadowEnabled"),sunEnabled_&&sunShadows_&&farShadowEnabled_?1:0);const auto normalizedSun=scene::normalize(sunDirection_);glapi::Uniform3f(uniformLocation(meshProgram_,"uSunDirection"),normalizedSun.x,normalizedSun.y,normalizedSun.z);glapi::Uniform1f(uniformLocation(meshProgram_,"uSunIntensity"),sunIntensity_);glapi::Uniform1f(uniformLocation(meshProgram_,"uSunAmbient"),sunAmbient_);const auto effectiveCamPos=(cameraPosition_.x!=0.0f||cameraPosition_.y!=0.0f||cameraPosition_.z!=0.0f)?cameraPosition_:shadowFocus_;glapi::Uniform3f(uniformLocation(meshProgram_,"uCameraPosition"),effectiveCamPos.x,effectiveCamPos.y,effectiveCamPos.z);glapi::Uniform1f(uniformLocation(meshProgram_,"uShadowDistance"),shadowDistance_);glapi::Uniform1f(uniformLocation(meshProgram_,"uShadowFadeStart"),std::min(shadowFadeStart_,shadowDistance_-1.0f));glapi::Uniform1f(uniformLocation(meshProgram_,"uFarShadowStart"),farShadowStart_);glapi::Uniform1f(uniformLocation(meshProgram_,"uFarShadowDistance"),farShadowDistance_);glapi::Uniform1f(uniformLocation(meshProgram_,"uFarShadowBlend"),farShadowBlend_);glapi::Uniform1i(uniformLocation(meshProgram_,"uShadowMap"),4);glapi::ActiveTexture(glapi::Texture0+4);glBindTexture(GL_TEXTURE_2D,shadowTexture_);glapi::Uniform1i(uniformLocation(meshProgram_,"uFarShadowMap"),14);glapi::ActiveTexture(glapi::Texture0+14);glBindTexture(GL_TEXTURE_2D,farShadowTexture_);
    glapi::Uniform1f(uniformLocation(meshProgram_,"uShadowIntensity"),shadowIntensity_);glapi::Uniform1f(uniformLocation(meshProgram_,"uShadowBias"),shadowBias_);glapi::Uniform1f(uniformLocation(meshProgram_,"uShadowNormalBias"),shadowNormalBias_);glapi::Uniform1f(uniformLocation(meshProgram_,"uShadowSoftness"),shadowSoftness_);glapi::Uniform1f(uniformLocation(meshProgram_,"uShadowContrast"),shadowContrast_);glapi::Uniform1i(uniformLocation(meshProgram_,"uViewmodelSelfShadows"),viewmodelSelfShadows_?1:0);glapi::Uniform1i(uniformLocation(meshProgram_,"uViewmodelShadowMap"),15);glapi::ActiveTexture(glapi::Texture0+15);glBindTexture(GL_TEXTURE_2D,viewmodelShadowTexture_);
    glapi::UniformMatrix4fv(meshUniforms_.viewmodelLightViewProjection,1,GL_FALSE,viewmodelLightViewProjection.data());
    glapi::Uniform3f(uniformLocation(meshProgram_,"uSunColor"),sunColor_.x,sunColor_.y,sunColor_.z);glapi::Uniform3f(uniformLocation(meshProgram_,"uAmbientColor"),ambientColor_.x,ambientColor_.y,ambientColor_.z);
    glapi::ActiveTexture(glapi::Texture0+2);glBindTexture(GL_TEXTURE_2D,camoTexture_);
    glapi::Uniform1f(meshUniforms_.eeveeMetallic, eeveeMetallic_);
    glapi::Uniform1f(meshUniforms_.eeveeRoughness, eeveeRoughness_);
    glapi::Uniform1f(meshUniforms_.eeveeIor, eeveeIor_);
    glapi::Uniform1f(meshUniforms_.eeveeSpecular, eeveeSpecular_);
    glapi::Uniform1f(meshUniforms_.eeveeSpecularTint, eeveeSpecularTint_);
    glapi::Uniform1f(meshUniforms_.eeveeClearcoat, eeveeClearcoat_);
    glapi::Uniform1f(meshUniforms_.eeveeClearcoatRoughness, eeveeClearcoatRoughness_);
    glapi::Uniform1i(meshUniforms_.brdfModel, brdfModel_);
    glapi::Uniform1f(meshUniforms_.shadowSpecularMultiplier,shadowSpecularMultiplier_);
    // Scoped to this mesh program pass; pose uploads only touch bone buffers.
    MeshUniformCache materialState;
    lastMaterialRequests_=lastMaterialUploads_=lastEmissionTextureBinds_=0;
    const auto set1i=[&](int p,int v){++lastMaterialRequests_;if(!meshStateCacheEnabled_||materialState.integer(p,v)){++lastMaterialUploads_;glapi::Uniform1i(p,v);}};
    const auto set1f=[&](int p,float v){++lastMaterialRequests_;if(!meshStateCacheEnabled_||materialState.scalar(p,v)){++lastMaterialUploads_;glapi::Uniform1f(p,v);}};
    const auto set3f=[&](int p,float x,float y,float z){++lastMaterialRequests_;if(!meshStateCacheEnabled_||materialState.vector3(p,x,y,z)){++lastMaterialUploads_;glapi::Uniform3f(p,x,y,z);}};
    const auto set4f=[&](int p,float x,float y,float z,float w){++lastMaterialRequests_;if(!meshStateCacheEnabled_||materialState.vector4(p,x,y,z,w)){++lastMaterialUploads_;glapi::Uniform4f(p,x,y,z,w);}};
    unsigned lastTex13=0xFFFFFFFF;
    int lastSurfaceProfile = -999;
    unsigned lastTex0 = 0xFFFFFFFF, lastTex1 = 0xFFFFFFFF, lastTex3 = 0xFFFFFFFF, lastTex5 = 0xFFFFFFFF, lastTex6 = 0xFFFFFFFF, lastVao = 0;
    int actorOverlayMode=0;scene::Vec4 actorOverlayColor{};float actorOverlayExpand=0;bool actorOverlayHidden=false;
    const auto overlayModeLoc=uniformLocation(meshProgram_,"uActorOverlay"),overlayColorLoc=uniformLocation(meshProgram_,"uActorOverlayColor"),overlayExpandLoc=uniformLocation(meshProgram_,"uOverlayExpand"),worldOpacityLoc=uniformLocation(meshProgram_,"uWorldOpacity");
    glapi::Uniform2f(uniformLocation(meshProgram_,"uOverlayViewport"),static_cast<float>(width),static_cast<float>(height));
    const auto drawMesh=[&](const GpuMesh& mesh,const scene::CastScene& drawScene,const std::vector<scene::Mat4>& drawPose){
        if(equipmentHidden(mesh,drawScene,drawPose))return;
        const bool mapSurface=&drawScene==mapScene;const bool viewmodelSurface=!mapSurface&&drawScene.skeleton.boneByCanonicalName.contains("tag_view");const bool weaponSurface=!mapSurface&&(mesh.viewmodelWeapon||mesh.camoBlend);const int requestedProfile=mapSurface?worldSpecularProfile_:(weaponSurface?viewmodelSpecularProfile_:playerSpecularProfile_);
        const int profileKey = requestedProfile * 10 + (weaponSurface ? 1 : 0) + (viewmodelSurface ? 2 : 0);
        if(profileKey != lastSurfaceProfile){
            lastSurfaceProfile = profileKey;
            const bool useAlternate=alternateSpecularEnabled_&&requestedProfile==1;
            set1i(meshUniforms_.weaponSurface,weaponSurface?1:0);
            set1f(meshUniforms_.weaponSpecularMultiplier,weaponSpecularMultiplier_);
            set1f(meshUniforms_.weaponCubemapMultiplier,weaponCubemapMultiplier_);
            set3f(meshUniforms_.weaponSpecularLow,weaponSpecularLow_.x,weaponSpecularLow_.y,weaponSpecularLow_.z);
            set3f(meshUniforms_.weaponSpecularHigh,weaponSpecularHigh_.x,weaponSpecularHigh_.y,weaponSpecularHigh_.z);
            set1f(meshUniforms_.specularIntensity,useAlternate?alternateSpecularIntensity_:specularIntensity_);
            set1f(meshUniforms_.specularSharpness,useAlternate?alternateSpecularSharpness_:specularSharpness_);
            set1f(meshUniforms_.specularIntensity2,useAlternate?alternateSpecularIntensity2_:specularIntensity2_);
            set1f(meshUniforms_.specularSharpness2,useAlternate?alternateSpecularSharpness2_:specularSharpness2_);
            set1f(meshUniforms_.lensAlpha,viewmodelSurface?lensAlpha_:worldLensAlpha_);
            const auto activeLensTint=viewmodelSurface?lensTint_:worldLensTint_;
            set3f(meshUniforms_.lensTint,activeLensTint.x,activeLensTint.y,activeLensTint.z);
            set1f(meshUniforms_.lensTintIntensity,viewmodelSurface?lensTintIntensity_:worldLensTintIntensity_);
            set1f(meshUniforms_.lensSpecularIntensity,viewmodelSurface?lensSpecularIntensity_:worldLensSpecularIntensity_);
            set1f(meshUniforms_.lensCubemapIntensity,viewmodelSurface?lensCubemapIntensity_:worldLensCubemapIntensity_);
        }
        if(!mapSurface){
            if(mesh.hideWhenCamo&&camoTexture_)return;
            auto model=mesh.model;
            if(mesh.attachmentIndex>=0&&static_cast<std::size_t>(mesh.attachmentIndex)<drawScene.attachments.size()){
                const auto& attachment=drawScene.attachments[mesh.attachmentIndex];
                if(attachment.boneIndex<drawPose.size())model=drawPose[attachment.boneIndex]*attachment.localMatrix()*model;
            }
            glapi::UniformMatrix4fv(meshUniforms_.model,1,GL_FALSE,model.data());
            set1i(meshUniforms_.ignoreAlpha,ignoreViewmodelTextureAlpha_?1:0);
            set1i(meshUniforms_.viewmodelSurface,&drawScene==&scene?1:0);
            set1i(meshUniforms_.gltfPbr,mesh.gltfPbr?1:0);
            set1f(meshUniforms_.gltfMetallic,mesh.metallicFactor);
            set1f(meshUniforms_.gltfRoughness,mesh.roughnessFactor);
            set1f(meshUniforms_.gltfTransmission,mesh.transmissionFactor);
            set1f(meshUniforms_.gltfIor,mesh.indexOfRefraction);
            set3f(meshUniforms_.gltfEmissive,mesh.emissiveFactor.x,mesh.emissiveFactor.y,mesh.emissiveFactor.z);
            set1i(meshUniforms_.skinned,mesh.skinned?1:0);
            bool useLuma = (camoLumaMask_ && weaponSurface);
            float lumaLow = camoLumaLow_, lumaHigh = camoLumaHigh_, lumaGamma = camoLumaGamma_, lumaContrast = camoLumaContrast_;
            bool invertCamo = camoInvert_;
            if(mesh.attachmentIndex >= 0){
                const auto attIt = attachmentCamoLumas_.find(mesh.attachmentIndex);
                if(attIt != attachmentCamoLumas_.end() && attIt->second.enabled){
                    useLuma = attIt->second.useBaseColorLumaMask;
                    invertCamo = attIt->second.invertCamoMask;
                    lumaLow = attIt->second.camoLumaLow;
                    lumaHigh = attIt->second.camoLumaHigh;
                    lumaGamma = attIt->second.camoLumaGamma;
                    lumaContrast = attIt->second.camoLumaContrast;
                }
            }
            set1i(meshUniforms_.camoLumaMask, useLuma ? 1 : 0);
            set1i(meshUniforms_.camoInvert, invertCamo ? 1 : 0);
            set1f(meshUniforms_.camoLumaLow, lumaLow);
            set1f(meshUniforms_.camoLumaHigh, lumaHigh);
            set1f(meshUniforms_.camoLumaGamma, lumaGamma);
            set1f(meshUniforms_.camoLumaContrast, lumaContrast);
            set1i(meshUniforms_.lens,mesh.lens&&!wireframe?1:0);
            set1i(meshUniforms_.hasSpecularImperfections,specularImperfectionsTexture_&&(&drawScene!=mapScene)&&!wireframe?1:0);
            set1i(meshUniforms_.camoBlend,(mesh.camoBlend||(universalCamo_&&weaponSurface&&!mesh.lens&&!mesh.emissive))&&!wireframe?1:0);
            set1i(meshUniforms_.camoUseAlpha,mesh.camoUseAlpha?1:0);
            set1i(meshUniforms_.camoMaskUseful,mesh.camoMaskUseful?1:0);
        }
        set1i(meshUniforms_.eyeOverlay,mesh.eyeOverlay&&!wireframe?1:0);
        set1i(meshUniforms_.gltfPbr,mesh.gltfPbr?1:0);
        set1f(meshUniforms_.gltfMetallic,mesh.metallicFactor);
        set1f(meshUniforms_.gltfRoughness,mesh.roughnessFactor);
        set1f(meshUniforms_.gltfTransmission,mesh.transmissionFactor);
        set1f(meshUniforms_.gltfIor,mesh.indexOfRefraction);
        set3f(meshUniforms_.gltfEmissive,mesh.emissiveFactor.x,mesh.emissiveFactor.y,mesh.emissiveFactor.z);
        set1i(meshUniforms_.emissive,mesh.emissive&&!wireframe?1:0);
        const auto color=wireframe?scene::Vec4{0.25f,0.85f,1.0f,1.0f}:mesh.color;
        set4f(meshUniforms_.color,color.x,color.y,color.z,color.w);
        set1i(meshUniforms_.hasAlbedo,mesh.texture&&!wireframe?1:0);
        set1i(meshUniforms_.hasNormalMap,mesh.normalTexture&&!wireframe?1:0);
        set1i(meshUniforms_.hasSpecularMap,mesh.specularTexture&&!wireframe?1:0);
        set1i(meshUniforms_.specularGlossiness,mesh.specularGlossiness&&!mesh.lens&&!wireframe?1:0);
        set1i(meshUniforms_.hasMetalnessMap,mesh.metalnessTexture&&!wireframe?1:0);
        set1i(meshUniforms_.source2Material,mesh.source2Material&&!wireframe?1:0);
        set1i(meshUniforms_.hasRoughnessMap,mesh.roughnessTexture&&!wireframe?1:0);
        set1i(meshUniforms_.forceAlpha,mesh.forceAlpha&&!wireframe?1:0);
        set1i(meshUniforms_.explicitMaterial,mesh.materialPolicyExplicit?1:0);
        set1i(meshUniforms_.unlit,mesh.unlit?1:0);
        set1i(meshUniforms_.vertexColor,mesh.useVertexColor?1:0);
        set1f(meshUniforms_.alphaCutoff,mesh.alphaCutoff);
        set1i(meshUniforms_.hasEmissionMap,mesh.emissiveTexture?1:0);
        // Explicit map materials do not use the legacy imperfection channel.
        const unsigned emissionTexture=mesh.materialPolicyExplicit?mesh.emissiveTexture:specularImperfectionsTexture_;
        if(!meshStateCacheEnabled_||lastTex13!=emissionTexture){
            glapi::ActiveTexture(glapi::Texture0+13);
            glBindTexture(GL_TEXTURE_2D,emissionTexture);
            lastTex13=emissionTexture;++lastEmissionTextureBinds_;
        }
        set1i(meshUniforms_.decal,mesh.decal&&!wireframe?1:0);
        set1i(meshUniforms_.decalMultiply,mesh.decalMultiply&&!wireframe?1:0);
        set1i(meshUniforms_.decalAdditive,mesh.decalAdditive&&!wireframe?1:0);
        set1i(meshUniforms_.alphaTest,mesh.alphaTest&&!wireframe?1:0);
        if(lastTex1 != mesh.texture){
            glapi::ActiveTexture(glapi::Texture0+1);
            glBindTexture(GL_TEXTURE_2D,mesh.texture);
            lastTex1 = mesh.texture;
        }
        if(lastTex0 != mesh.metalnessTexture){glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,mesh.metalnessTexture);lastTex0=mesh.metalnessTexture;}
        if(lastTex3 != mesh.roughnessTexture){glapi::ActiveTexture(glapi::Texture0+3);glBindTexture(GL_TEXTURE_2D,mesh.roughnessTexture);lastTex3=mesh.roughnessTexture;}
        if(lastTex5 != mesh.normalTexture){
            glapi::ActiveTexture(glapi::Texture0+5);
            glBindTexture(GL_TEXTURE_2D,mesh.normalTexture);
            lastTex5 = mesh.normalTexture;
        }
        if(lastTex6 != mesh.specularTexture){
            glapi::ActiveTexture(glapi::Texture0+6);
            glBindTexture(GL_TEXTURE_2D,mesh.specularTexture);
            lastTex6 = mesh.specularTexture;
        }
        if(lastVao != mesh.vao){
            glapi::BindVertexArray(mesh.vao);
            lastVao = mesh.vao;
        }
        const bool useWire = (wireframe||actorOverlayMode==3) && mesh.wireIndexBuffer != 0;
        glapi::BindBuffer(glapi::ElementArrayBuffer,useWire?mesh.wireIndexBuffer:mesh.indexBuffer);
        const bool cullBefore=glIsEnabled(GL_CULL_FACE)!=0;
        if(mesh.materialPolicyExplicit&&!wireframe){if(mesh.doubleSided)glDisable(GL_CULL_FACE);else{glEnable(GL_CULL_FACE);glCullFace(GL_BACK);}}
        if(mesh.decal&&!wireframe){
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-2.5f,mesh.materialPolicyExplicit&&mesh.materialDepthBias!=0?mesh.materialDepthBias:-std::clamp(decalDepthBias_+6.0f,6.0f,16.0f));
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND);
            if(mesh.decalAdditive) glBlendFunc(mesh.materialPolicyExplicit&&mesh.sourceBlend==1&&mesh.destinationBlend==1?GL_ONE:GL_SRC_ALPHA,GL_ONE);
            else if(mesh.decalMultiply) glBlendFunc(GL_DST_COLOR,GL_ZERO);
            else glapi::BlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        }
        const bool alphaBlend=!wireframe&&(mesh.forceAlpha||mesh.decalAdditive||mesh.decalMultiply)&&!mesh.decal;
        if(alphaBlend){
            glEnable(GL_BLEND);
            if(mesh.eyeOverlay) glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
            else if(mesh.decalAdditive) glBlendFunc(mesh.materialPolicyExplicit&&mesh.sourceBlend==1&&mesh.destinationBlend==1?GL_ONE:GL_SRC_ALPHA,GL_ONE);
            else if(mesh.decalMultiply) glBlendFunc(GL_DST_COLOR,GL_ZERO);
            else glapi::BlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        }
        set1i(overlayModeLoc,actorOverlayMode);set4f(overlayColorLoc,actorOverlayColor.x,actorOverlayColor.y,actorOverlayColor.z,actorOverlayColor.w);set1f(overlayExpandLoc,actorOverlayExpand);
        const bool transparentMap=mapSurface&&actorOverlays_.transparentWorld;
        set1f(worldOpacityLoc,transparentMap?actorOverlays_.worldOpacity:1.f);
        if(actorOverlayMode||transparentMap){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);}
        if(actorOverlayMode){glDepthFunc(actorOverlayHidden?GL_GREATER:GL_LEQUAL);if(actorOverlayExpand>0){glEnable(GL_CULL_FACE);glCullFace(GL_FRONT);}}
        glapi::DrawElements(useWire?GL_LINES:GL_TRIANGLES,useWire?mesh.wireIndexCount:mesh.indexCount,GL_UNSIGNED_INT,nullptr);
        if(actorOverlayMode||transparentMap){glDepthMask(GL_TRUE);glDisable(GL_BLEND);}
        if(actorOverlayMode){glDepthFunc(GL_LESS);glCullFace(GL_BACK);if(cullBefore)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);}

        if(mesh.materialPolicyExplicit&&!wireframe){if(cullBefore)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);}
        lastTotalDrawCalls_++;
        if(&drawScene == mapScene) lastVisibleMapMeshes_++;
        if(alphaBlend){glDepthMask(GL_TRUE);glDisable(GL_BLEND);}
        if(mesh.decal&&!wireframe){glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDepthFunc(GL_LESS);glDisable(GL_POLYGON_OFFSET_FILL);}
    };

    const auto drawOverlayWorld=[&]{
    if(!viewmodelCapture_&&mapScene&&(!mapOpaqueMeshes_.empty()||!mapDecalMeshes_.empty()||!mapTransparentMeshes_.empty()||!mapAdditiveMeshes_.empty())){
        const auto cameraFrustum=scene::extractFrustum(viewProjection);
        // Set invariant map uniforms once before map passes
        glapi::UniformMatrix4fv(meshUniforms_.model,1,GL_FALSE,scene::Mat4::identity().data());
        glapi::Uniform1i(meshUniforms_.ignoreAlpha,ignoreMapTextureAlpha_?1:0);
        glapi::Uniform1i(meshUniforms_.viewmodelSurface,0);
        glapi::Uniform1i(meshUniforms_.weaponSurface,0);
        glapi::Uniform1i(meshUniforms_.skinned,0);
        glapi::Uniform1i(meshUniforms_.gltfPbr,0);
        glapi::Uniform1f(meshUniforms_.gltfMetallic,0.0f);
        glapi::Uniform1f(meshUniforms_.gltfRoughness,0.85f);
        glapi::Uniform1f(meshUniforms_.gltfTransmission,0.0f);
        glapi::Uniform1f(meshUniforms_.gltfIor,1.0f);
        glapi::Uniform3f(meshUniforms_.gltfEmissive,0.0f,0.0f,0.0f);
        glapi::Uniform1i(meshUniforms_.camoLumaMask,0);
        glapi::Uniform1i(meshUniforms_.camoInvert,0);
        glapi::Uniform1i(meshUniforms_.camoBlend,0);
        glapi::Uniform1i(meshUniforms_.camoUseAlpha,0);
        glapi::Uniform1i(meshUniforms_.camoMaskUseful,0);
        glapi::Uniform1i(meshUniforms_.hasSpecularImperfections,0);
        glapi::Uniform1i(meshUniforms_.lens,0);
        glapi::Uniform1i(meshUniforms_.emissive,0);
        // Explicit queue first; alpha-like surfaces within a queue back-to-front.
        const auto sortPass=[&](std::vector<std::size_t>& pass,bool blended){
            if(sortPreparationEnabled_&&blended){sortTransparentPass(pass,meshes_,cameraPosition_,sortDistances_);return;}
            if(std::none_of(pass.begin(),pass.end(),[&](std::size_t i){return meshes_[i].materialPolicyExplicit;}))return;
            std::stable_sort(pass.begin(),pass.end(),[&](std::size_t a,std::size_t b){
                const auto& x=meshes_[a];const auto& y=meshes_[b];
                const int xq=x.renderQueue<0?2000:x.renderQueue,yq=y.renderQueue<0?2000:y.renderQueue;
                if(xq!=yq)return xq<yq;
                if(!blended)return x.alphaTest<y.alphaTest;
                const auto xc=(x.aabbMin+x.aabbMax)*.5f-cameraPosition_,yc=(y.aabbMin+y.aabbMax)*.5f-cameraPosition_;
                return scene::dot(xc,xc)>scene::dot(yc,yc);
            });
        };
        // Opaque order is finalized at upload; blended order follows the camera.
        sortPass(mapDecalMeshes_,true);
        sortPass(mapTransparentMeshes_,true);sortPass(mapAdditiveMeshes_,true);
        // Pass 0: Opaque world geometry (hardware backface culling enabled for massive fillrate boost)
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        auto& fadedWorldOrder=fadedWorldOrder_;
        if(actorOverlays_.transparentWorld){
            fadedWorldOrder=mapOpaqueMeshes_;
            if(sortPreparationEnabled_){
                sortDistances_.resize(meshes_.size());
                for(const auto i:fadedWorldOrder){const auto d=meshes_[i].sortKey.center-cameraPosition_;sortDistances_[i]=scene::dot(d,d);}
                std::stable_sort(fadedWorldOrder.begin(),fadedWorldOrder.end(),[&](std::size_t a,std::size_t b){return sortDistances_[a]>sortDistances_[b];});
            }else std::stable_sort(fadedWorldOrder.begin(),fadedWorldOrder.end(),[&](std::size_t a,std::size_t b){const auto ac=(meshes_[a].aabbMin+meshes_[a].aabbMax)*.5f-cameraPosition_,bc=(meshes_[b].aabbMin+meshes_[b].aabbMax)*.5f-cameraPosition_;return scene::dot(ac,ac)>scene::dot(bc,bc);});
        }
        for(const auto meshIdx : (actorOverlays_.transparentWorld?fadedWorldOrder:mapOpaqueMeshes_)){
            if(meshIdx >= meshes_.size()) continue;
            const auto& m=meshes_[meshIdx];
            if(m.hasBounds&&!scene::isAabbInFrustum(cameraFrustum,m.aabbMin,m.aabbMax))continue;
            drawMesh(m,*mapScene,{});
        }
        glDisable(GL_CULL_FACE);
        // Pass 1: Decals (posters, graffiti, screens, flowers on walls, multiply decals)
        for(const auto meshIdx : mapDecalMeshes_){
            if(meshIdx >= meshes_.size()) continue;
            const auto& m=meshes_[meshIdx];
            if(m.hasBounds&&!scene::isAabbInFrustum(cameraFrustum,m.aabbMin,m.aabbMax))continue;
            drawMesh(m,*mapScene,{});
        }
        if(water_.enabled){renderWater(viewProjection,lightViewProjection,farLightViewProjection);lastVao=0;}
        // Pass 2: Transparent surfaces (water, waterfall sheets, glass windows, alpha cutouts)
        for(const auto meshIdx : mapTransparentMeshes_){
            if(meshIdx >= meshes_.size()) continue;
            const auto& m=meshes_[meshIdx];
            if(m.hasBounds&&!scene::isAabbInFrustum(cameraFrustum,m.aabbMin,m.aabbMax))continue;
            drawMesh(m,*mapScene,{});
        }
        // Pass 3: Additive surfaces (godrays, flares, beams)
        for(const auto meshIdx : mapAdditiveMeshes_){
            if(meshIdx >= meshes_.size()) continue;
            const auto& m=meshes_[meshIdx];
            if(m.hasBounds&&!scene::isAabbInFrustum(cameraFrustum,m.aabbMin,m.aabbMax))continue;
            drawMesh(m,*mapScene,{});
        }
    }
    };
    if(!actorOverlays_.transparentWorld)drawOverlayWorld();
    const auto drawActor=[&](const scene::CastScene& drawScene,const std::vector<scene::Mat4>& actorPose,std::size_t first,std::size_t count,int variant){
        if(!count||first+count>meshes_.size())return;
        if(!actorVisible(drawScene,actorPose,first,count,variant,cameraFrustum))return;
        uploadPose(drawScene,actorPose);
        lastTex1 = 0xFFFFFFFF; lastTex5 = 0xFFFFFFFF; lastTex6 = 0xFFFFFFFF; lastVao = 0; lastSurfaceProfile = -999;
        for(int pass=0;pass<3;++pass)for(std::size_t i=first;i<first+count;++i)if(((pass==0&&!meshes_[i].forceAlpha&&!meshes_[i].lens)||(pass==1&&meshes_[i].forceAlpha&&!meshes_[i].lens)||(pass==2&&meshes_[i].lens))&&(variant<0||meshes_[i].actorVariant<0||meshes_[i].actorVariant==variant))drawMesh(meshes_[i],drawScene,actorPose);
    };
    if(!viewmodelCapture_&&worldActorScene&&worldActorPose)drawActor(*worldActorScene,*worldActorPose,activeWorldFirst,activeWorldCount,-1);
    if(!viewmodelCapture_&&actorScene&&actorPoses&&actorMeshCount_&&actorMeshFirst_+actorMeshCount_<=meshes_.size())for(std::size_t actorIndex=0;actorIndex<actorPoses->size();++actorIndex){const auto& actorPose=(*actorPoses)[actorIndex];const int variant=actorVariants&&actorIndex<actorVariants->size()?(*actorVariants)[actorIndex]:-1;
        drawActor(*actorScene,actorPose,actorMeshFirst_,actorMeshCount_,variant);
    }
    std::vector<float> actorFxLines,actorFxBoxes,actorFxRibbon;
    overlayHistory_.begin(overlayTime_,actorOverlays_.historyEnabled());
    const auto ring=[&](scene::Vec3 p,float radius,scene::Vec4 color){
        for(int i=0;i<48;++i){float a=i*scene::kPi/24,b=(i+1)*scene::kPi/24;pushLine(actorFxLines,p+scene::Vec3{std::cos(a)*radius,std::sin(a)*radius,1},p+scene::Vec3{std::cos(b)*radius,std::sin(b)*radius,1},color);}
    };
    const auto cube=[&](scene::Vec3 low,scene::Vec3 high,scene::Vec4 color){
        scene::Vec3 c[8];for(int i=0;i<8;++i)c[i]={(i&1)?high.x:low.x,(i&2)?high.y:low.y,(i&4)?high.z:low.z};
        for(int i=0;i<8;++i)for(int bit:{1,2,4})if(!(i&bit))pushLine(actorFxLines,c[i],c[i|bit],color);
    };
    const auto overlayActor=[&](const scene::CastScene& drawScene,const std::vector<scene::Mat4>& actorPose,std::size_t first,std::size_t count,int variant,ActorOverlayInput input){
        if(!(actorOverlays_.chams||actorOverlays_.outlines||actorOverlays_.glow||actorOverlays_.boxes||actorOverlays_.historyEnabled())||actorPose.size()!=drawScene.skeleton.bones.size()||actorPose.empty()||!count)return;
        scene::Vec3 low{1e20f,1e20f,1e20f},high{-1e20f,-1e20f,-1e20f};
        for(std::size_t i=0;i<actorPose.size();++i){
            const auto& name=drawScene.skeleton.bones[i].name;
            if(name.find("tag_")!=std::string::npos||name.find("weapon")!=std::string::npos)continue;
            const auto& m=actorPose[i];scene::Vec3 p{m.v[12],m.v[13],m.v[14]};
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))continue;
            low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
        }
        if(low.x>high.x)return;
        const scene::Vec3 root{(low.x+high.x)*.5f,(low.y+high.y)*.5f,low.z};
        auto* history=overlayHistory_.update(input,actorPose,root,actorOverlays_);
        const auto pass=[&](const std::vector<scene::Mat4>& p,int mode,scene::Vec4 color,float expand,bool hidden){
            // Histories are still updated off-screen; each old pose has its own
            // bounds, and expanded outlines/glow use a padded camera frustum.
            // History eviction may recycle addresses within this frame; only
            // cache live poses whose storage remains stable throughout render.
            if(!actorVisible(drawScene,p,first,count,variant,visibility::paddedFrustum(viewProjection,std::max(0.f,expand)+2.f,width,height),&p==&actorPose))return;
            actorOverlayMode=mode;actorOverlayColor=color;actorOverlayExpand=expand;actorOverlayHidden=hidden;
            uploadPose(drawScene,p);lastTex1=lastTex5=lastTex6=0xFFFFFFFF;lastVao=0;lastSurfaceProfile=-999;
            for(std::size_t i=first;i<first+count&&i<meshes_.size();++i)if((variant<0||meshes_[i].actorVariant<0||meshes_[i].actorVariant==variant)&&!meshes_[i].lens&&!meshes_[i].eyeOverlay)drawMesh(meshes_[i],drawScene,p);
        };
        if(actorOverlays_.chams){
            auto c=actorOverlays_.hidden;c.w*=actorOverlays_.opacity;
            if(actorOverlays_.throughWalls){
                // Exclude the visible actor surface, not just its front-most triangle: hidden colors must not leak through translucent Fresnel/wireframe.
                glEnable(GL_STENCIL_TEST);glStencilMask(0xFF);glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);glStencilFunc(GL_ALWAYS,1,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);pass(actorPose,1,{1,1,1,1},0,false);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glStencilMask(0);glStencilFunc(GL_NOTEQUAL,1,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);pass(actorPose,actorOverlays_.material+1,c,0,true);glDisable(GL_STENCIL_TEST);glStencilMask(0xFF);
            }
            c=actorOverlays_.visible;c.w*=actorOverlays_.opacity;pass(actorPose,actorOverlays_.material+1,c,0,false);
        }
        if(actorOverlays_.outlines||actorOverlays_.glow){glEnable(GL_STENCIL_TEST);glStencilMask(0xFF);glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);glStencilFunc(GL_ALWAYS,1,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glDisable(GL_DEPTH_TEST);pass(actorPose,1,{1,1,1,1},0,false);glEnable(GL_DEPTH_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glStencilMask(0);glStencilFunc(GL_NOTEQUAL,1,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);}
        if(actorOverlays_.glow)for(int i=4;i>=1;--i){auto c=actorOverlays_.edge;c.w*=actorOverlays_.glowStrength*.18f;pass(actorPose,1,c,actorOverlays_.outlineWidth+actorOverlays_.glowWidth*i/4.f,false);if(actorOverlays_.throughWalls)pass(actorPose,1,c,actorOverlays_.outlineWidth+actorOverlays_.glowWidth*i/4.f,true);}
        if(actorOverlays_.outlines){pass(actorPose,1,actorOverlays_.edge,actorOverlays_.outlineWidth,false);if(actorOverlays_.throughWalls)pass(actorPose,1,actorOverlays_.edge,actorOverlays_.outlineWidth,true);}
        if(actorOverlays_.outlines||actorOverlays_.glow){glDisable(GL_STENCIL_TEST);glStencilMask(0xFF);}
        if(history){
            if(actorOverlays_.echoes){int n=0;for(auto i=history->samples.rbegin();i!=history->samples.rend()&&n<actorOverlays_.echoCount;++i)if(!i->pose.empty()&&overlayTime_-i->time>.025){auto c=actorOverlays_.motion;c.w*=float(1-(overlayTime_-i->time)/actorOverlays_.historySeconds)*.6f;pass(i->pose,1,c,0,false);++n;}}
            for(std::size_t i=1;i<history->samples.size();++i){const auto& a=history->samples[i-1];const auto& b=history->samples[i];if(actorOverlays_.ribbons||(actorOverlays_.arcs&&(a.airborne||b.airborne))){auto c=actorOverlays_.motion;c.w*=float(1-(overlayTime_-a.time)/actorOverlays_.historySeconds);const auto p=a.root+scene::Vec3{0,0,2},q=b.root+scene::Vec3{0,0,2};if(actorOverlays_.ribbons&&scene::length(q-p)>.001f){const auto mid=(p+q)*.5f;auto side=scene::cross(q-p,cameraPosition_-mid);if(scene::length(side)>.001f){side=scene::normalize(side)*actorOverlays_.lineWidth*std::max(1.f,scene::length(cameraPosition_-mid))/std::max(1,height);pushTriangle(actorFxRibbon,p-side,p+side,q+side,c);pushTriangle(actorFxRibbon,p-side,q+side,q-side,c);}}if(actorOverlays_.arcs&&(a.airborne||b.airborne))pushLine(actorFxLines,p,q,c);}}
            const double landAge=overlayTime_-history->landingTime;
            if(actorOverlays_.landingRings&&landAge>=0&&landAge<.65){auto c=actorOverlays_.motion;c.w*=float(1-landAge/.65);ring(history->landing,actorOverlays_.ringRadius*float(.2+landAge/.65),c);}
            const double hitAge=overlayTime_-history->hitTime;
            if(hitAge>=0&&hitAge<actorOverlays_.hitDuration){
                auto c=actorOverlays_.hit;c.w*=float(1-hitAge/actorOverlays_.hitDuration);
                if(actorOverlays_.hitGhosts&&!history->hitPose.empty())pass(history->hitPose,1,c,0,false);
                if(actorOverlays_.impactPulses)for(std::size_t i=0;i<actorPose.size();++i){const auto& bone=drawScene.skeleton.bones[i];auto parent=bone.parent;if(parent<0||static_cast<std::size_t>(parent)>=actorPose.size()||bone.name.find("tag_")!=std::string::npos||bone.name.find("weapon")!=std::string::npos)continue;const auto&a=actorPose[i];const auto&b=actorPose[parent];pushLine(actorFxLines,{a.v[12],a.v[13],a.v[14]},{b.v[12],b.v[13],b.v[14]},c);}
            }
        }
        if(actorOverlays_.boxes){
            auto c=actorOverlays_.box;if(actorOverlays_.rainbow){float t=float(overlayTime_*actorOverlays_.rainbowSpeed*scene::kPi*2);c.x=.5f+.5f*std::sin(t);c.y=.5f+.5f*std::sin(t+2.0944f);c.z=.5f+.5f*std::sin(t+4.1888f);}
            float x0=1e20f,y0=1e20f,x1=-1e20f,y1=-1e20f;bool good=true;
            for(int i=0;i<8;++i){scene::Vec3 p{(i&1)?high.x:low.x,(i&2)?high.y:low.y,(i&4)?high.z:low.z};float w=viewProjection.v[3]*p.x+viewProjection.v[7]*p.y+viewProjection.v[11]*p.z+viewProjection.v[15];if(w<=.001){good=false;break;}float x=(viewProjection.v[0]*p.x+viewProjection.v[4]*p.y+viewProjection.v[8]*p.z+viewProjection.v[12])/w,y=(viewProjection.v[1]*p.x+viewProjection.v[5]*p.y+viewProjection.v[9]*p.z+viewProjection.v[13])/w;x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);}
            if(good){pushLine(actorFxBoxes,{x0,y0,0},{x1,y0,0},c);pushLine(actorFxBoxes,{x1,y0,0},{x1,y1,0},c);pushLine(actorFxBoxes,{x1,y1,0},{x0,y1,0},c);pushLine(actorFxBoxes,{x0,y1,0},{x0,y0,0},c);}
        }
        actorOverlayMode=0;actorOverlayExpand=0;actorOverlayHidden=false;
    };
    if(!viewmodelCapture_){
        if(worldActorScene&&worldActorPose)overlayActor(*worldActorScene,*worldActorPose,activeWorldFirst,activeWorldCount,-1,overlayPlayer_);
        if(actorScene&&actorPoses)for(std::size_t i=0;i<actorPoses->size();++i)overlayActor(*actorScene,(*actorPoses)[i],actorMeshFirst_,actorMeshCount_,actorVariants&&i<actorVariants->size()?(*actorVariants)[i]:-1,i<overlayInputs_.size()?overlayInputs_[i]:ActorOverlayInput{i+1});
    }
    set1i(overlayModeLoc,0);set1f(overlayExpandLoc,0);set1f(worldOpacityLoc,1);
    if(actorOverlays_.transparentWorld){materialState={};lastSurfaceProfile=-999;drawOverlayWorld();materialState={};lastSurfaceProfile=-999;}
    while(!overlayImpacts_.empty()&&(overlayTime_<overlayImpacts_.front().time||overlayTime_-overlayImpacts_.front().time>actorOverlays_.cubeDuration))overlayImpacts_.pop_front();
    if(actorOverlays_.impactCubes&&!viewmodelCapture_)for(const auto& impact:overlayImpacts_){auto c=actorOverlays_.hit;c.w*=float(1-(overlayTime_-impact.time)/actorOverlays_.cubeDuration);const scene::Vec3 half{actorOverlays_.cubeSize,actorOverlays_.cubeSize,actorOverlays_.cubeSize};cube(impact.position-half,impact.position+half,c);}

    // First-person weapons use the engine's foreground depth range: world and
    // actors render first, then the view rig occupies only the nearest slice.
    // Unlike clearing depth, this preserves world depth for later debug lines.
    if(showMainScene){
        if(firstPersonProjection_)glapi::Uniform2f(foregroundClipLoc,foreground_depth::nearPlane,foreground_depth::farPlane);
        uploadPose(scene,globalPose);
        lastTex1 = 0xFFFFFFFF; lastTex5 = 0xFFFFFFFF; lastTex6 = 0xFFFFFFFF; lastVao = 0; lastSurfaceProfile = -999;
        glDepthFunc(GL_LEQUAL);
        glDepthRange(0.0,0.04);
        for(int pass=0;pass<3;++pass)
            for(std::size_t i=activeMainFirst;i<std::min(activeMainFirst+activeMainCount,meshes_.size());++i)
                if((pass==0&&!meshes_[i].forceAlpha&&!meshes_[i].lens)||(pass==1&&meshes_[i].forceAlpha&&!meshes_[i].lens)||(pass==2&&meshes_[i].lens))
                    drawMesh(meshes_[i],scene,globalPose);
        glDepthRange(0.0,1.0);
        glDepthFunc(GL_LESS);
        glapi::Uniform2f(foregroundClipLoc,0,0);
    }

    for(auto& scratch:overlayScratch_)scratch.clear();
    auto& lines=overlayScratch_[0];auto& solids=overlayScratch_[1];auto& navigationOverlayLines=overlayScratch_[2];
    auto& actorOccludedLines=overlayScratch_[3];auto& actorVisibleLines=overlayScratch_[4];auto& actorHitboxLines=overlayScratch_[5];auto& navigationLines=navigationOverlayLines;
    if(showGrid&&!viewmodelCapture_){
        constexpr float radius=scene::course::kHalfExtent,step=scene::course::scaled(32.0f);constexpr int count=static_cast<int>(radius/step);
        for(int i=-count;i<=count;++i){const float p=i*step;const bool major=i%8==0;const scene::Vec4 c=i==0?scene::Vec4{1.0f,0.42f,0.04f,1}:major?scene::Vec4{0.68f,0.25f,0.035f,0.9f}:scene::Vec4{0.28f,0.105f,0.018f,0.72f};
            pushLine(lines,{-radius,p,0},{radius,p,0},c);pushLine(lines,{p,-radius,0},{p,radius,0},c);}
        const scene::Vec4 boundary{0.92f,0.42f,0.06f,1.0f},courseColor{0.72f,0.28f,0.055f,1.0f},brushColor{0.46f,0.18f,0.045f,1.0f};
        pushLine(lines,{-radius,-radius,0},{radius,-radius,0},boundary);pushLine(lines,{radius,-radius,0},{radius,radius,0},boundary);
        pushLine(lines,{radius,radius,0},{-radius,radius,0},boundary);pushLine(lines,{-radius,radius,0},{-radius,-radius,0},boundary);
        for(const auto& box:scene::course::kBoxes){pushSolidBox(solids,box,brushColor);pushBox(lines,box,courseColor);}
        const scene::Vec3 ramp[6]={{scene::course::scaled(128),scene::course::scaled(-352),0},{scene::course::scaled(128),scene::course::scaled(-192),0},{scene::course::scaled(384),scene::course::scaled(-352),scene::course::scaled(64)},{scene::course::scaled(384),scene::course::scaled(-192),scene::course::scaled(64)},{scene::course::scaled(128),scene::course::scaled(-352),0},{scene::course::scaled(128),scene::course::scaled(-192),0}};
        pushTriangle(solids,ramp[0],ramp[2],ramp[3],brushColor);pushTriangle(solids,ramp[0],ramp[3],ramp[1],brushColor);pushLine(lines,ramp[0],ramp[2],courseColor);pushLine(lines,ramp[1],ramp[3],courseColor);pushLine(lines,ramp[0],ramp[1],courseColor);pushLine(lines,ramp[2],ramp[3],courseColor);
        for(int i=1;i<8;++i){const float x=scene::course::scaled(128.0f+i*32.0f),z=scene::course::rampHeight(x);pushLine(lines,{x,scene::course::scaled(-352),z},{x,scene::course::scaled(-192),z},scene::Vec4{0.62f,0.22f,0.025f,0.9f});}
    }
    if(showSkeleton&&globalPose.size()==scene.skeleton.bones.size()){
        for(std::size_t i=0;i<globalPose.size();++i){const auto parent=scene.skeleton.bones[i].parent;if(parent<0)continue;
            pushLine(lines,{globalPose[parent].v[12],globalPose[parent].v[13],globalPose[parent].v[14]},
                           {globalPose[i].v[12],globalPose[i].v[13],globalPose[i].v[14]},{1.0f,0.58f,0.12f,1});}
    }
    if(showSkeleton&&actorScene&&actorPoses)for(const auto& actorPose:*actorPoses)if(actorPose.size()==actorScene->skeleton.bones.size())for(std::size_t i=0;i<actorPose.size();++i){const auto parent=actorScene->skeleton.bones[i].parent;if(parent<0)continue;pushLine(lines,{actorPose[parent].v[12],actorPose[parent].v[13],actorPose[parent].v[14]},{actorPose[i].v[12],actorPose[i].v[13],actorPose[i].v[14]},{1.0f,0.2f,0.12f,1});}
    if(actorScene&&actorPoses&&(wallhack_||hitboxes_))for(const auto& actorPose:*actorPoses)if(actorPose.size()==actorScene->skeleton.bones.size()){scene::Vec3 minimum{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()},maximum{-minimum.x,-minimum.y,-minimum.z};for(std::size_t i=0;i<actorPose.size();++i){const scene::Vec3 p{actorPose[i].v[12],actorPose[i].v[13],actorPose[i].v[14]};minimum={std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};maximum={std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};const auto parent=actorScene->skeleton.bones[i].parent;if(wallhack_&&parent>=0){const scene::Vec3 q{actorPose[static_cast<std::size_t>(parent)].v[12],actorPose[static_cast<std::size_t>(parent)].v[13],actorPose[static_cast<std::size_t>(parent)].v[14]};pushLine(actorOccludedLines,q,p,{wallhackOccludedColor_.x,wallhackOccludedColor_.y,wallhackOccludedColor_.z,1});pushLine(actorVisibleLines,q,p,{wallhackVisibleColor_.x,wallhackVisibleColor_.y,wallhackVisibleColor_.z,1});}}if(hitboxes_&&minimum.x<maximum.x){const scene::course::Box box{minimum.x,minimum.y,maximum.x,maximum.y,maximum.z-minimum.z};auto& local=overlayScratch_[7];local.clear();pushBox(local,box,{hitboxColor_.x,hitboxColor_.y,hitboxColor_.z,1});for(std::size_t v=0;v+6<local.size();v+=7){local[v+2]+=minimum.z;local[v+5]+=minimum.z;}actorHitboxLines.insert(actorHitboxLines.end(),local.begin(),local.end());}}
    if(showSkeleton&&worldActorScene&&worldActorPose&&worldActorPose->size()==worldActorScene->skeleton.bones.size())for(std::size_t i=0;i<worldActorPose->size();++i){const auto parent=worldActorScene->skeleton.bones[i].parent;if(parent<0)continue;pushLine(lines,{(*worldActorPose)[parent].v[12],(*worldActorPose)[parent].v[13],(*worldActorPose)[parent].v[14]},{(*worldActorPose)[i].v[12],(*worldActorPose)[i].v[13],(*worldActorPose)[i].v[14]},{0.2f,0.75f,1.0f,1});}
    if(navigation){
        if(navigation->boundary.enabled&&!navigation->boundary.points.empty()){
            const auto& poly=navigation->boundary;
            const auto polyColor=scene::Vec4{1.0f,0.55f,0.15f,0.95f};
            const auto floorColor=scene::Vec4{1.0f,0.22f,0.12f,0.75f};
            const auto ceilColor=scene::Vec4{0.2f,0.85f,1.0f,0.75f};
            const std::size_t n=poly.points.size();
            for(std::size_t i=0;i<n;++i){
                const std::size_t next=(i+1)%n;
                const scene::Vec3 p0{poly.points[i].x,poly.points[i].y,poly.floorZ};
                const scene::Vec3 p1{poly.points[next].x,poly.points[next].y,poly.floorZ};
                const scene::Vec3 q0{poly.points[i].x,poly.points[i].y,poly.ceilingZ};
                const scene::Vec3 q1{poly.points[next].x,poly.points[next].y,poly.ceilingZ};
                pushLine(navigationLines,p0,p1,floorColor);
                pushLine(navigationLines,q0,q1,ceilColor);
                pushLine(navigationLines,p0,q0,polyColor);
            }
        }
        for(const auto& link:navigation->links)if(link.from<navigation->nodes.size()&&link.to<navigation->nodes.size()){
            const auto color=link.bidirectional?scene::Vec4{navigationLinkColor_.x,navigationLinkColor_.y,navigationLinkColor_.z,.95f}:scene::Vec4{1.0f,.62f,.12f,.95f};const auto a=navigation->nodes[link.from].position+scene::Vec3{0,0,7},b=navigation->nodes[link.to].position+scene::Vec3{0,0,7};const int segments=navigationInterpolation_>.001f?12:1;scene::Vec3 previous=a;for(int segment=1;segment<=segments;++segment){const float t=static_cast<float>(segment)/segments;scene::Vec3 point=a*(1.0f-t)+b*t;if(segments>1)point.z+=std::sin(t*scene::kPi)*scene::length(b-a)*.08f*navigationInterpolation_;pushLine(navigationLines,previous,point,color);previous=point;}}
        for(std::size_t nodeIndex=0;nodeIndex<navigation->nodes.size();++nodeIndex){const auto& node=navigation->nodes[nodeIndex];const auto color=static_cast<int>(nodeIndex)==selectedNavigationNode?scene::Vec4{1,1,1,1}:node.disabled?scene::Vec4{.35f,.35f,.35f,1}:node.priority?scene::Vec4{navigationPriorityColor_.x,navigationPriorityColor_.y,navigationPriorityColor_.z,1}:scene::Vec4{navigationNodeColor_.x,navigationNodeColor_.y,navigationNodeColor_.z,1};const int segments=navigationCircleSides_;const float radius=node.radius*navigationCircleScale_;for(int segment=0;segment<segments;++segment){const float a=2.0f*scene::kPi*segment/segments,b=2.0f*scene::kPi*(segment+1)/segments;pushLine(navigationLines,node.position+scene::Vec3{std::cos(a)*radius,std::sin(a)*radius,5},node.position+scene::Vec3{std::cos(b)*radius,std::sin(b)*radius,5},color);}const float stem=navigationHandleHeight_+std::clamp(node.weight,0.0f,10.0f)*12.0f;pushLine(navigationLines,node.position,node.position+scene::Vec3{0,0,stem},color);pushLine(navigationLines,node.position+scene::Vec3{-navigationCrossLength_,0,stem},node.position+scene::Vec3{navigationCrossLength_,0,stem},color);pushLine(navigationLines,node.position+scene::Vec3{0,-navigationCrossLength_,stem},node.position+scene::Vec3{0,navigationCrossLength_,stem},color);}
        for(std::size_t blockIndex=0;blockIndex<navigation->blocks.size();++blockIndex){const auto& block=navigation->blocks[blockIndex];const auto color=static_cast<int>(blockIndex)==selectedNavigationBlock?scene::Vec4{1,.25f,.15f,1}:scene::Vec4{.9f,.05f,.05f,.9f};const scene::Vec3 p000{block.minimum.x,block.minimum.y,block.minimum.z},p100{block.maximum.x,block.minimum.y,block.minimum.z},p110{block.maximum.x,block.maximum.y,block.minimum.z},p010{block.minimum.x,block.maximum.y,block.minimum.z},p001{block.minimum.x,block.minimum.y,block.maximum.z},p101{block.maximum.x,block.minimum.y,block.maximum.z},p111{block.maximum.x,block.maximum.y,block.maximum.z},p011{block.minimum.x,block.maximum.y,block.maximum.z};pushLine(navigationLines,p000,p100,color);pushLine(navigationLines,p100,p110,color);pushLine(navigationLines,p110,p010,color);pushLine(navigationLines,p010,p000,color);pushLine(navigationLines,p001,p101,color);pushLine(navigationLines,p101,p111,color);pushLine(navigationLines,p111,p011,color);pushLine(navigationLines,p011,p001,color);pushLine(navigationLines,p000,p001,color);pushLine(navigationLines,p100,p101,color);pushLine(navigationLines,p110,p111,color);pushLine(navigationLines,p010,p011,color);pushLine(navigationLines,p001,p111,color);pushLine(navigationLines,p101,p011,color);}
        for(std::size_t goalIndex=0;goalIndex<navigation->goalAreas.size();++goalIndex){
            const auto& goal=navigation->goalAreas[goalIndex];
            if(!goal.enabled)continue;
            const auto color=scene::Vec4{0.82f,0.28f,1.0f,0.92f};
            const int segments=32;
            for(int segment=0;segment<segments;++segment){
                const float a=2.0f*scene::kPi*segment/segments,b=2.0f*scene::kPi*(segment+1)/segments;
                pushLine(navigationLines,goal.position+scene::Vec3{std::cos(a)*goal.radius,std::sin(a)*goal.radius,5},
                                         goal.position+scene::Vec3{std::cos(b)*goal.radius,std::sin(b)*goal.radius,5},color);
            }
            const float stem=60.0f+std::clamp(goal.weight,0.0f,10.0f)*8.0f;
            pushLine(navigationLines,goal.position,goal.position+scene::Vec3{0,0,stem},color);
            pushLine(navigationLines,goal.position+scene::Vec3{-20,0,stem},goal.position+scene::Vec3{20,0,stem},color);
            pushLine(navigationLines,goal.position+scene::Vec3{0,-20,stem},goal.position+scene::Vec3{0,20,stem},color);
            pushLine(navigationLines,goal.position+scene::Vec3{-14,-14,stem},goal.position+scene::Vec3{14,14,stem},color);
            pushLine(navigationLines,goal.position+scene::Vec3{-14,14,stem},goal.position+scene::Vec3{14,-14,stem},color);
        }
    }
    if(botSpawns){
        const scene::Vec4 spawnPinColor{0.18f,0.92f,0.38f,1.0f};
        const scene::Vec4 spawnBaseColor{0.25f,1.0f,0.55f,0.95f};
        for(const auto& spawn:*botSpawns){
            const float r=20.0f;
            pushLine(navigationLines,spawn+scene::Vec3{-r,0,3},spawn+scene::Vec3{0,r,3},spawnBaseColor);
            pushLine(navigationLines,spawn+scene::Vec3{0,r,3},spawn+scene::Vec3{r,0,3},spawnBaseColor);
            pushLine(navigationLines,spawn+scene::Vec3{r,0,3},spawn+scene::Vec3{0,-r,3},spawnBaseColor);
            pushLine(navigationLines,spawn+scene::Vec3{0,-r,3},spawn+scene::Vec3{-r,0,3},spawnBaseColor);
            pushLine(navigationLines,spawn,spawn+scene::Vec3{0,0,65.0f},spawnPinColor);
            pushLine(navigationLines,spawn+scene::Vec3{-10,0,65.0f},spawn+scene::Vec3{10,0,65.0f},spawnPinColor);
            pushLine(navigationLines,spawn+scene::Vec3{0,-10,65.0f},spawn+scene::Vec3{0,10,65.0f},spawnPinColor);
            pushLine(navigationLines,spawn+scene::Vec3{0,0,65.0f},spawn+scene::Vec3{0,0,72.0f},spawnPinColor);
        }
    }
    if(navigation)for(std::size_t vertex=0;vertex+6<navigationLines.size();vertex+=7){auto& r=navigationLines[vertex+3];auto& g=navigationLines[vertex+4];auto& b=navigationLines[vertex+5];const auto close=[](float a,float value){return std::abs(a-value)<.002f;};if(close(r,.15f)&&close(g,.72f)&&close(b,1.0f)){r=navigationLinkColor_.x;g=navigationLinkColor_.y;b=navigationLinkColor_.z;}else if(close(r,.1f)&&close(g,.9f)&&close(b,1.0f)){r=navigationNodeColor_.x;g=navigationNodeColor_.y;b=navigationNodeColor_.z;}else if(close(r,1.0f)&&close(g,.82f)&&close(b,.12f)){r=navigationPriorityColor_.x;g=navigationPriorityColor_.y;b=navigationPriorityColor_.z;}}
    auto& campathLines=overlayScratch_[6];
    if(campathOverlay&&!campathOverlay->empty()){
        const auto& keys=*campathOverlay;
        const scene::Vec4 splineColor{campathSplineColor_.x,campathSplineColor_.y,campathSplineColor_.z,0.95f};
        const scene::Vec4 nodeColor{campathNodeColor_.x,campathNodeColor_.y,campathNodeColor_.z,1.0f};
        const scene::Vec4 selectedColor{1.0f,1.0f,1.0f,1.0f};
        const scene::Vec4 frustumColor{campathFrustumColor_.x,campathFrustumColor_.y,campathFrustumColor_.z,0.85f};
        if(keys.size()>=2){
            const float firstTick=static_cast<float>(keys.front().tick),lastTick=static_cast<float>(keys.back().tick);
            const int sampleCount=std::max(20,static_cast<int>((keys.size()-1)*32));
            const float tickStep=(lastTick-firstTick)/static_cast<float>(sampleCount);
            scene::Vec3 prevPos=keys.front().position;
            float accumulatedDist=0.0f;
            for(int s=1;s<=sampleCount;++s){
                const float t=firstTick+s*tickStep;
                const auto sample=take::interpolateDollyCamera(keys,t);
                const float segDist=scene::length(sample.position-prevPos);
                accumulatedDist+=segDist;
                if(!campathDashed_||std::fmod(accumulatedDist,std::max(2.0f,campathDashLength_*2.0f))<campathDashLength_){
                    pushLine(campathLines,prevPos,sample.position,splineColor);
                }
                prevPos=sample.position;
            }
        }
        for(std::size_t i=0;i<keys.size();++i){
            const auto& node=keys[i];
            const bool selected=(static_cast<int>(i)==selectedCampathNode);
            const auto col=selected?selectedColor:nodeColor;
            const float sz=selected?14.0f:9.0f;
            pushLine(campathLines,node.position+scene::Vec3{-sz,0,0},node.position+scene::Vec3{sz,0,0},col);
            pushLine(campathLines,node.position+scene::Vec3{0,-sz,0},node.position+scene::Vec3{0,sz,0},col);
            pushLine(campathLines,node.position+scene::Vec3{0,0,-sz},node.position+scene::Vec3{0,0,sz},col);
            pushLine(campathLines,node.position+scene::Vec3{-sz*.7f,-sz*.7f,0},node.position+scene::Vec3{sz*.7f,sz*.7f,0},col);
            pushLine(campathLines,node.position+scene::Vec3{-sz*.7f,sz*.7f,0},node.position+scene::Vec3{sz*.7f,-sz*.7f,0},col);
            const float pitchRad=node.rotationDegrees.x*scene::kPi/180.0f,yawRad=node.rotationDegrees.y*scene::kPi/180.0f,rollRad=node.rotationDegrees.z*scene::kPi/180.0f;
            const scene::Vec3 forward{std::cos(yawRad)*std::cos(pitchRad),std::sin(yawRad)*std::cos(pitchRad),-std::sin(pitchRad)};
            const auto lat=scene::normalize(scene::cross(scene::Vec3{0,0,1},forward));
            const auto upVec=scene::normalize(scene::cross(forward,lat));
            const auto rolledUp=scene::normalize(upVec*std::cos(rollRad)+lat*std::sin(rollRad));
            const auto rolledRight=scene::normalize(scene::cross(forward,rolledUp));
            const float coneDist=selected?36.0f:24.0f,halfW=coneDist*std::tan(node.fov*0.5f*scene::kPi/180.0f)*0.75f,halfH=halfW*0.5625f;
            const scene::Vec3 coneCenter=node.position+forward*coneDist;
            const scene::Vec3 tl=coneCenter-rolledRight*halfW+rolledUp*halfH,tr=coneCenter+rolledRight*halfW+rolledUp*halfH,bl=coneCenter-rolledRight*halfW-rolledUp*halfH,br=coneCenter+rolledRight*halfW-rolledUp*halfH;
            pushLine(campathLines,node.position,tl,frustumColor);pushLine(campathLines,node.position,tr,frustumColor);pushLine(campathLines,node.position,bl,frustumColor);pushLine(campathLines,node.position,br,frustumColor);
            pushLine(campathLines,tl,tr,frustumColor);pushLine(campathLines,tr,br,frustumColor);pushLine(campathLines,br,bl,frustumColor);pushLine(campathLines,bl,tl,frustumColor);
        }
        if(activeCameraSample){
            const auto& cam=*activeCameraSample;
            const float pitchRad=cam.rotationDegrees.x*scene::kPi/180.0f,yawRad=cam.rotationDegrees.y*scene::kPi/180.0f,rollRad=cam.rotationDegrees.z*scene::kPi/180.0f;
            const scene::Vec3 forward{std::cos(yawRad)*std::cos(pitchRad),std::sin(yawRad)*std::cos(pitchRad),-std::sin(pitchRad)};
            const auto lat=scene::normalize(scene::cross(scene::Vec3{0,0,1},forward));
            const auto upVec=scene::normalize(scene::cross(forward,lat));
            const auto rolledUp=scene::normalize(upVec*std::cos(rollRad)+lat*std::sin(rollRad));
            const auto rolledRight=scene::normalize(scene::cross(forward,rolledUp));

            const scene::Vec4 activeCamColor{1.0f,0.3f,0.3f,1.0f};
            const scene::Vec4 activeRayColor{1.0f,0.85f,0.25f,0.95f};
            const scene::Vec4 activeFrustumColor{0.2f,0.95f,1.0f,0.95f};

            const float bw=6.5f,bh=4.5f,bd=7.5f;
            const scene::Vec3 bodyBack=cam.position-forward*bd;
            const scene::Vec3 b_ftl=cam.position-rolledRight*bw+rolledUp*bh, b_ftr=cam.position+rolledRight*bw+rolledUp*bh;
            const scene::Vec3 b_fbl=cam.position-rolledRight*bw-rolledUp*bh, b_fbr=cam.position+rolledRight*bw-rolledUp*bh;
            const scene::Vec3 b_btl=bodyBack-rolledRight*bw+rolledUp*bh, b_btr=bodyBack+rolledRight*bw+rolledUp*bh;
            const scene::Vec3 b_bbl=bodyBack-rolledRight*bw-rolledUp*bh, b_bbr=bodyBack+rolledRight*bw-rolledUp*bh;
            pushLine(campathLines,b_ftl,b_ftr,activeCamColor);pushLine(campathLines,b_ftr,b_fbr,activeCamColor);pushLine(campathLines,b_fbr,b_fbl,activeCamColor);pushLine(campathLines,b_fbl,b_ftl,activeCamColor);
            pushLine(campathLines,b_btl,b_btr,activeCamColor);pushLine(campathLines,b_btr,b_bbr,activeCamColor);pushLine(campathLines,b_bbr,b_bbl,activeCamColor);pushLine(campathLines,b_bbl,b_btl,activeCamColor);
            pushLine(campathLines,b_ftl,b_btl,activeCamColor);pushLine(campathLines,b_ftr,b_btr,activeCamColor);pushLine(campathLines,b_fbl,b_bbl,activeCamColor);pushLine(campathLines,b_fbr,b_bbr,activeCamColor);

            const float activeConeDist=40.0f;
            const float activeHalfW=activeConeDist*std::tan(cam.fov*0.5f*scene::kPi/180.0f)*0.75f,activeHalfH=activeHalfW*0.5625f;
            const scene::Vec3 activeConeCenter=cam.position+forward*activeConeDist;
            const scene::Vec3 a_tl=activeConeCenter-rolledRight*activeHalfW+rolledUp*activeHalfH, a_tr=activeConeCenter+rolledRight*activeHalfW+rolledUp*activeHalfH;
            const scene::Vec3 a_bl=activeConeCenter-rolledRight*activeHalfW-rolledUp*activeHalfH, a_br=activeConeCenter+rolledRight*activeHalfW-rolledUp*activeHalfH;
            pushLine(campathLines,cam.position,a_tl,activeFrustumColor);pushLine(campathLines,cam.position,a_tr,activeFrustumColor);pushLine(campathLines,cam.position,a_bl,activeFrustumColor);pushLine(campathLines,cam.position,a_br,activeFrustumColor);
            pushLine(campathLines,a_tl,a_tr,activeFrustumColor);pushLine(campathLines,a_tr,a_br,activeFrustumColor);pushLine(campathLines,a_br,a_bl,activeFrustumColor);pushLine(campathLines,a_bl,a_tl,activeFrustumColor);

            const scene::Vec3 aimEnd=cam.position+forward*160.0f;
            pushLine(campathLines,cam.position,aimEnd,activeRayColor);
            pushLine(campathLines,aimEnd-rolledRight*6.0f,aimEnd+rolledRight*6.0f,activeRayColor);
            pushLine(campathLines,aimEnd-rolledUp*6.0f,aimEnd+rolledUp*6.0f,activeRayColor);
        }
    }
    if(!actorFxRibbon.empty()){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);drawTriangles(actorFxRibbon,viewProjection);glDepthMask(GL_TRUE);glDisable(GL_BLEND);}
    if(!actorFxLines.empty()){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glLineWidth(actorOverlays_.lineWidth);drawLines(actorFxLines,viewProjection);glDepthMask(GL_TRUE);glDisable(GL_BLEND);}
    if(!actorFxBoxes.empty()){glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glLineWidth(actorOverlays_.boxWidth);drawLines(actorFxBoxes,scene::Mat4::identity());glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);}
    drawTriangles(solids,viewProjection);glLineWidth(1.5f);drawLines(lines,viewProjection);if(!actorOccludedLines.empty()){glDisable(GL_DEPTH_TEST);glLineWidth(wallhackThickness_);drawLines(actorOccludedLines,viewProjection);glEnable(GL_DEPTH_TEST);glLineWidth(wallhackThickness_);drawLines(actorVisibleLines,viewProjection);}if(!actorHitboxLines.empty()){glLineWidth(hitboxThickness_);drawLines(actorHitboxLines,viewProjection);}if(!navigationOverlayLines.empty()){if(navigationOnTop)glDisable(GL_DEPTH_TEST);glLineWidth(navigationLineThickness_);drawLines(navigationOverlayLines,viewProjection);if(navigationOnTop)glEnable(GL_DEPTH_TEST);}
    if(collisionOverlayEnabled_&&collisionOverlayMap_){
        const auto& map=*collisionOverlayMap_;std::vector<float> overlay;
        const auto emit=[&](const scene::glb::CollisionTriangle& t,scene::Vec3 color){
            const auto nearest=scene::Vec3{std::clamp(cameraPosition_.x,t.minimum.x,t.maximum.x),std::clamp(cameraPosition_.y,t.minimum.y,t.maximum.y),std::clamp(cameraPosition_.z,t.minimum.z,t.maximum.z)};
            if(scene::length(nearest-cameraPosition_)>collisionOverlayRange_)return;
            for(auto p:{t.a,t.b,t.b,t.c,t.c,t.a}){p=p+t.normal*.35f;overlay.insert(overlay.end(),{p.x,p.y,p.z,color.x,color.y,color.z,.72f});}
        };
        std::vector<std::uint32_t> indices=map.globalCollision;
        if(map.gridWidth>0&&!map.gridCollision.empty()){
            for(int y=std::max(0,map.gridCoordY(cameraPosition_.y-collisionOverlayRange_));y<=std::min(map.gridHeight-1,map.gridCoordY(cameraPosition_.y+collisionOverlayRange_));++y)
            for(int x=std::max(0,map.gridCoordX(cameraPosition_.x-collisionOverlayRange_));x<=std::min(map.gridWidth-1,map.gridCoordX(cameraPosition_.x+collisionOverlayRange_));++x){const auto& cell=map.gridCollision[map.cellIndex(x,y)];indices.insert(indices.end(),cell.begin(),cell.end());}
            std::sort(indices.begin(),indices.end());indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
        }else{indices.resize(map.collision.size());for(std::size_t i=0;i<indices.size();++i)indices[i]=static_cast<std::uint32_t>(i);}
        for(auto i:indices){const auto& t=map.collision[i];emit(t,t.walkable?scene::Vec3{.15f,1.f,.3f}:scene::Vec3{1.f,.35f,.08f});}
        for(const auto& t:map.authoredTriggerTriangles)emit(t,{.85f,.2f,1.f});
        glDepthMask(GL_FALSE);if(collisionOverlayThroughWalls_)glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glLineWidth(1.5f);drawLines(overlay,viewProjection);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
    }
    if(!campathLines.empty()){glLineWidth(campathThickness_);drawLines(campathLines,viewProjection);}
    glapi::BindVertexArray(0);glapi::UseProgram(0);
    if(msaaEnabled_){glapi::BindFramebuffer(glapi::ReadFramebuffer,msaaFramebuffer_);glapi::BindFramebuffer(glapi::DrawFramebuffer,framebuffer_);glapi::BlitFramebuffer(0,0,width_,height_,0,0,width_,height_,GL_COLOR_BUFFER_BIT,GL_NEAREST);glapi::BlitFramebuffer(0,0,width_,height_,0,0,width_,height_,GL_DEPTH_BUFFER_BIT,GL_NEAREST);}
    renderVolumetricLighting(viewProjection,lightViewProjection,farLightViewProjection,showMainScene);
    #include "render/AoTransparencyPass.inc"
    const bool hbaoActive=hbao_.enabled&&(debugView_==0||debugView_==1)&&renderHbao(viewProjection,showMainScene);
    if(dof_.enabled&&hbaoActive){glapi::UseProgram(dofProgram_);configureAoFog(dofProgram_,viewProjection,showMainScene);}
    const bool dofActive=dof_.enabled&&(debugView_==0||debugView_==1)&&!(hbao_.enabled&&hbao_.preview)&&renderDepthOfField(showMainScene,hbaoActive,aoIsolationActive);
    if(postProgram_&&(dofActive||hbaoActive||debugView_==7||bloomEnabled_||lutTexture_||filmEnabled_||vignetteEnabled_||autoBlackPoint_||autoWhitePoint_||std::abs(lensDistortion_)>0.00001f||tonemappingMode_>0)){
        glapi::UseProgram(postProgram_);glapi::Uniform1i(uniformLocation(postProgram_,"uDebugView"),debugView_);glapi::Uniform1f(uniformLocation(postProgram_,"uDebugDepthNear"),debugDepthNear_);glapi::Uniform1f(uniformLocation(postProgram_,"uDebugDepthFar"),debugDepthFar_);glapi::Uniform1i(uniformLocation(postProgram_,"uDebugDepthInvert"),debugDepthInvert_?1:0);
        glapi::Uniform1i(uniformLocation(postProgram_,"uSeparateForeground"),firstPersonProjection_&&foregroundDrawn_);glapi::Uniform2f(uniformLocation(postProgram_,"uViewmodelDepthRange"),foreground_depth::nearPlane,foreground_depth::farPlane);
        glDisable(GL_DEPTH_TEST);glapi::BindVertexArray(lineVao_);
        if(bloomEnabled_&&kawaseProgram_&&bloomFramebuffer_){
            glapi::BindFramebuffer(glapi::Framebuffer,bloomFramebuffer_);glapi::UseProgram(kawaseProgram_);glapi::Uniform1i(uniformLocation(kawaseProgram_,"uSource"),0);glapi::Uniform1f(uniformLocation(kawaseProgram_,"uThresholdSoftness"),bloomSoftThreshold_);glapi::Uniform1f(uniformLocation(kawaseProgram_,"uSaturationBias"),bloomSaturationBias_);glapi::Uniform1f(uniformLocation(kawaseProgram_,"uAspect"),bloomAspect_);glapi::Uniform1f(uniformLocation(kawaseProgram_,"uRotation"),bloomRotation_*scene::kPi/180.0f);glapi::ActiveTexture(glapi::Texture0);glDisable(GL_BLEND);
            std::size_t targetLevels = 1;
            if (bloomRadius_ > 0.15f) targetLevels = 2;
            if (bloomRadius_ > 0.6f) targetLevels = 3;
            if (bloomRadius_ > 1.5f) targetLevels = 4;
            if (bloomRadius_ > 3.0f) targetLevels = 5;
            if (bloomRadius_ > 6.0f) targetLevels = 6;
            const std::size_t numLevels = std::min<std::size_t>(bloomTextures_.size(),
                std::min<std::size_t>(targetLevels, std::max<std::size_t>(1, static_cast<std::size_t>(bloomKawaseSamples_))));
            const float kawaseOffset = std::max(0.02f, bloomRadius_ * 0.35f);
            for(std::size_t level=0;level<numLevels;++level){
                const int sourceWidth=level?bloomWidths_[level-1]:width_,sourceHeight=level?bloomHeights_[level-1]:height_;
                glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,bloomTextures_[level],0);
                glViewport(0,0,bloomWidths_[level],bloomHeights_[level]);
                glBindTexture(GL_TEXTURE_2D,level?bloomTextures_[level-1]:colorTexture_);
                glapi::Uniform2f(uniformLocation(kawaseProgram_,"uTexel"),1.0f/sourceWidth,1.0f/sourceHeight);
                glapi::Uniform1f(uniformLocation(kawaseProgram_,"uOffset"),kawaseOffset*(1.0f+static_cast<float>(level)*0.4f));
                glapi::Uniform1i(uniformLocation(kawaseProgram_,"uThreshold"),level==0?1:0);
                glapi::Uniform1f(uniformLocation(kawaseProgram_,"uThresholdValue"),bloomThreshold_);
                glapi::DrawArrays(GL_TRIANGLES,0,3);
            }
            if(numLevels > 1){
                glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);
                for(std::size_t level=numLevels-1;level>0;--level){
                    glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,bloomTextures_[level-1],0);
                    glViewport(0,0,bloomWidths_[level-1],bloomHeights_[level-1]);
                    glBindTexture(GL_TEXTURE_2D,bloomTextures_[level]);
                    glapi::Uniform2f(uniformLocation(kawaseProgram_,"uTexel"),1.0f/bloomWidths_[level],1.0f/bloomHeights_[level]);
                    glapi::Uniform1f(uniformLocation(kawaseProgram_,"uOffset"),kawaseOffset*(1.0f+static_cast<float>(level)*0.4f));
                    glapi::Uniform1i(uniformLocation(kawaseProgram_,"uThreshold"),0);
                    glapi::DrawArrays(GL_TRIANGLES,0,3);
                }
                glDisable(GL_BLEND);
            }
        }
        glapi::UseProgram(postProgram_);
        configureAoFog(postProgram_,viewProjection,showMainScene);
        glapi::Uniform1i(uniformLocation(postProgram_,"uDofEnabled"),dofActive);glapi::Uniform1i(uniformLocation(postProgram_,"uDofPreview"),dof_.preview);glapi::Uniform1i(uniformLocation(postProgram_,"uAoIsolationEnabled"),aoIsolationActive);
        glapi::Uniform1i(uniformLocation(postProgram_,"uDofFar"),5);glapi::ActiveTexture(glapi::Texture0+5);glBindTexture(GL_TEXTURE_2D,dofTextures_[0]);glapi::Uniform1i(uniformLocation(postProgram_,"uDofNear"),6);glapi::ActiveTexture(glapi::Texture0+6);glBindTexture(GL_TEXTURE_2D,dofTextures_[1]);glapi::Uniform1i(uniformLocation(postProgram_,"uAoIsolated"),7);glapi::ActiveTexture(glapi::Texture0+7);glBindTexture(GL_TEXTURE_2D,aoIsolationTexture_);
        glapi::Uniform1i(uniformLocation(postProgram_,"uHbaoEnabled"),hbaoActive?1:0);glapi::Uniform1i(uniformLocation(postProgram_,"uHbaoPreview"),hbao_.preview?1:0);
        glapi::Uniform1f(uniformLocation(postProgram_,"uHbaoIntensity"),hbao_.intensity);glapi::Uniform1f(uniformLocation(postProgram_,"uHbaoPower"),hbao_.power);
        glapi::Uniform1i(uniformLocation(postProgram_,"uHbao"),4);glapi::ActiveTexture(glapi::Texture0+4);glBindTexture(GL_TEXTURE_2D,hbaoActive?hbaoTextures_[0]:0);
        glapi::UseProgram(postProgram_);glapi::Uniform2f(uniformLocation(postProgram_,"uTexel"),1.0f/width_,1.0f/height_);glapi::Uniform1i(uniformLocation(postProgram_,"uTonemapping"),tonemappingMode_);glapi::Uniform1i(uniformLocation(postProgram_,"uBloomBlendMode"),bloomBlendMode_);glapi::Uniform1iv(uniformLocation(postProgram_,"uPostPassOrder"),7,postPassOrder_.data());glapi::Uniform1i(uniformLocation(postProgram_,"uTonemapRec709Match"),tonemapRec709Match_?1:0);glapi::Uniform1f(uniformLocation(postProgram_,"uTonemapRec709Strength"),tonemapRec709Strength_);glapi::Uniform1f(uniformLocation(postProgram_,"uLutIntensity"),lutIntensity_);glapi::Uniform1i(uniformLocation(postProgram_,"uFilmEnabled"),filmEnabled_?1:0);glapi::Uniform1f(uniformLocation(postProgram_,"uFilmBrightness"),filmBrightness_);glapi::Uniform1f(uniformLocation(postProgram_,"uFilmContrast"),filmContrast_);glapi::Uniform1f(uniformLocation(postProgram_,"uFilmDesaturation"),filmDesaturation_);glapi::Uniform3f(uniformLocation(postProgram_,"uFilmDarkTint"),filmDarkTint_.x,filmDarkTint_.y,filmDarkTint_.z);glapi::Uniform3f(uniformLocation(postProgram_,"uFilmMidTint"),filmMidTint_.x,filmMidTint_.y,filmMidTint_.z);glapi::Uniform3f(uniformLocation(postProgram_,"uFilmLightTint"),filmLightTint_.x,filmLightTint_.y,filmLightTint_.z);glapi::Uniform1i(uniformLocation(postProgram_,"uFilmMidTintEnabled"),filmMidTintEnabled_?1:0);glapi::Uniform1i(uniformLocation(postProgram_,"uFilmInvert"),filmInvert_?1:0);glapi::Uniform1i(uniformLocation(postProgram_,"uVignetteEnabled"),vignetteEnabled_?1:0);glapi::Uniform1f(uniformLocation(postProgram_,"uVignetteIntensity"),vignetteIntensity_);glapi::Uniform1f(uniformLocation(postProgram_,"uVignetteRadius"),vignetteRadius_);glapi::Uniform1f(uniformLocation(postProgram_,"uVignetteSoftness"),vignetteSoftness_);glapi::Uniform1i(uniformLocation(postProgram_,"uAutoBlack"),autoBlackPoint_?1:0);glapi::Uniform1f(uniformLocation(postProgram_,"uAutoBlackIntensity"),autoBlackPointIntensity_);glapi::Uniform1i(uniformLocation(postProgram_,"uAutoWhite"),autoWhitePoint_?1:0);glapi::Uniform1f(uniformLocation(postProgram_,"uAutoWhiteIntensity"),autoWhitePointIntensity_);
        glapi::BindFramebuffer(glapi::Framebuffer,postFramebuffer_);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,postTexture_,0);glViewport(0,0,width_,height_);glapi::UseProgram(postProgram_);glapi::Uniform1i(uniformLocation(postProgram_,"uColor"),0);glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,colorTexture_);glapi::Uniform1i(uniformLocation(postProgram_,"uDepth"),16);glapi::ActiveTexture(glapi::Texture0+16);glBindTexture(GL_TEXTURE_2D,depthBuffer_);glapi::Uniform1f(uniformLocation(postProgram_,"uCameraNear"),cameraNear_);glapi::Uniform1f(uniformLocation(postProgram_,"uCameraFar"),cameraFar_);glapi::Uniform1i(uniformLocation(postProgram_,"uBloomTexture"),1);glapi::ActiveTexture(glapi::Texture0+1);glBindTexture(GL_TEXTURE_2D,bloomTextures_[0]);glapi::Uniform1i(uniformLocation(postProgram_,"uLut"),3);glapi::ActiveTexture(glapi::Texture0+3);glBindTexture(GL_TEXTURE_2D,lutTexture_);glapi::Uniform1i(uniformLocation(postProgram_,"uBloom"),bloomEnabled_?1:0);glapi::Uniform1f(uniformLocation(postProgram_,"uBloomIntensity"),bloomIntensity_);glapi::Uniform1i(uniformLocation(postProgram_,"uLutEnabled"),lutTexture_?1:0);glapi::Uniform1f(uniformLocation(postProgram_,"uLutSize"),static_cast<float>(std::max(2,lutSize_)));glapi::Uniform1f(uniformLocation(postProgram_,"uDistortion"),lensDistortion_);glapi::DrawArrays(GL_TRIANGLES,0,3);glapi::BindFramebuffer(glapi::ReadFramebuffer,postFramebuffer_);glapi::BindFramebuffer(glapi::DrawFramebuffer,framebuffer_);glapi::BlitFramebuffer(0,0,width_,height_,0,0,width_,height_,GL_COLOR_BUFFER_BIT,GL_NEAREST);glEnable(GL_DEPTH_TEST);
    }
    glapi::BindFramebuffer(glapi::Framebuffer,0);
}

bool StageRenderer::savePixelsPng(const std::filesystem::path& path,int width,int height,const std::vector<std::uint8_t>& rgbaPixels,std::string& error){
#ifdef _WIN32
    error.clear();if(!imageFactory_||width<=0||height<=0||rgbaPixels.empty()){error="No pixels available";return false;}
    const auto stride=static_cast<std::size_t>(width)*4,byteCount=stride*static_cast<std::size_t>(height);
    if(byteCount>static_cast<std::size_t>(std::numeric_limits<UINT>::max())||rgbaPixels.size()<byteCount){error="Pixel buffer size mismatch";return false;}
    std::vector<std::uint8_t> pixels=rgbaPixels;
    for(int y=0;y<height/2;++y){auto* top=pixels.data()+static_cast<std::size_t>(y)*stride;auto* bottom=pixels.data()+static_cast<std::size_t>(height-1-y)*stride;
        for(std::size_t x=0;x<stride;++x)std::swap(top[x],bottom[x]);}
    for(std::size_t i=0;i+3<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
    using Microsoft::WRL::ComPtr;auto* factory=static_cast<IWICImagingFactory*>(imageFactory_);ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> properties;
    if(FAILED(factory->CreateStream(&stream))||FAILED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE))||
       FAILED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))||FAILED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache))||
       FAILED(encoder->CreateNewFrame(&frame,&properties))||FAILED(frame->Initialize(properties.Get()))||FAILED(frame->SetSize(width,height))){error="Could not initialize PNG encoder";return false;}
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;if(FAILED(frame->SetPixelFormat(&format))||format!=GUID_WICPixelFormat32bppBGRA||
       FAILED(frame->WritePixels(height,static_cast<UINT>(stride),static_cast<UINT>(byteCount),pixels.data()))||FAILED(frame->Commit())||FAILED(encoder->Commit())){
        error="Could not encode PNG frame";return false;}
    return true;
#else
    (void)path;(void)width;(void)height;(void)rgbaPixels;error="PNG export is not implemented on this platform";return false;
#endif
}

bool StageRenderer::saveColorPng(const std::filesystem::path& path,std::string& error){
    std::vector<std::uint8_t> pixels;
    if(!readColorRgba(pixels,error))return false;
    return savePixelsPng(path,width_,height_,pixels,error);
}

bool StageRenderer::readColorRgba(std::vector<std::uint8_t>& pixels,std::string& error){error.clear();if(!framebuffer_||width_<=0||height_<=0){error="No rendered frame is available";return false;}const auto byteCount=static_cast<std::size_t>(width_)*static_cast<std::size_t>(height_)*4;if(byteCount>static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())*16ull){error="Rendered frame is too large";return false;}pixels.resize(byteCount);glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width_,height_,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glapi::BindFramebuffer(glapi::Framebuffer,0);return true;}

bool StageRenderer::captureReShadeSwapchain(GLFWwindow* window, std::vector<std::uint8_t>& outPixels, std::string& error) {
    error.clear();
    if (!window) {
        error = "No GLFW window available for ReShade swapchain capture";
        return false;
    }
    if (!framebuffer_ || width_ <= 0 || height_ <= 0) {
        error = "No rendered frame is available";
        return false;
    }
    glapi::BindFramebuffer(glapi::ReadFramebuffer, framebuffer_);
    glapi::BindFramebuffer(glapi::DrawFramebuffer, 0);
    glViewport(0, 0, width_, height_);
    glapi::BlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glapi::BindFramebuffer(glapi::Framebuffer, 0);
    glfwSwapBuffers(window);
    glReadBuffer(GL_FRONT);
    outPixels.resize(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, outPixels.data());
    glReadBuffer(GL_BACK);
    glapi::BindFramebuffer(glapi::Framebuffer, framebuffer_);
    return true;
}

bool StageRenderer::readDepthRgba(std::vector<std::uint8_t>& pixels,float projectionNear,float projectionFar,float rangeNear,float rangeFar,bool invert,std::string& error){error.clear();if(!framebuffer_||width_<=0||height_<=0){error="No rendered depth is available";return false;}projectionNear=std::max(0.0001f,projectionNear);projectionFar=std::max(projectionNear+0.001f,projectionFar);rangeNear=std::max(0.0f,rangeNear);rangeFar=std::max(rangeNear+0.001f,rangeFar);std::vector<float> depth(static_cast<std::size_t>(width_)*static_cast<std::size_t>(height_));glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width_,height_,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());glapi::BindFramebuffer(glapi::Framebuffer,0);pixels.resize(depth.size()*4);for(std::size_t i=0;i<depth.size();++i){float output{};if(depth[i]<1.0f){const double linear=foreground_depth::linearize(depth[i],projectionNear,projectionFar,foregroundDrawn_&&firstPersonProjection_,firstPersonProjection_);const double normalized=std::clamp((linear-rangeNear)/(rangeFar-rangeNear),0.0,1.0);output=static_cast<float>(1.0-normalized);}if(invert)output=1.0f-output;const auto value=static_cast<std::uint8_t>(std::round(output*255.0f));pixels[i*4]=pixels[i*4+1]=pixels[i*4+2]=value;pixels[i*4+3]=255;}return true;}

bool StageRenderer::readDepthRawFloat(std::vector<float>& depths,float projectionNear,float projectionFar,float rangeNear,float rangeFar,bool invert,std::string& error){
    error.clear();if(!framebuffer_||width_<=0||height_<=0){error="No rendered depth is available";return false;}
    projectionNear=std::max(0.0001f,projectionNear);projectionFar=std::max(projectionNear+0.001f,projectionFar);rangeNear=std::max(0.0f,rangeNear);rangeFar=std::max(rangeNear+0.001f,rangeFar);
    depths.resize(static_cast<std::size_t>(width_)*static_cast<std::size_t>(height_));
    glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width_,height_,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());glapi::BindFramebuffer(glapi::Framebuffer,0);
    for(std::size_t i=0;i<depths.size();++i){
        float output{};
        if(depths[i]<1.0f){
            const double linear=foreground_depth::linearize(depths[i],projectionNear,projectionFar,foregroundDrawn_&&firstPersonProjection_,firstPersonProjection_);
            const double normalized=std::clamp((linear-rangeNear)/(rangeFar-rangeNear),0.0,1.0);
            output=static_cast<float>(1.0-normalized);
        }
        if(invert)output=1.0f-output;
        depths[i]=output;
    }
    return true;
}

bool StageRenderer::saveDepthImage(const std::filesystem::path& path,int formatBitDepth,float projectionNear,float projectionFar,float rangeNear,float rangeFar,bool invert,std::string& error){
#ifdef _WIN32
    error.clear();if(!imageFactory_||!framebuffer_||width_<=0||height_<=0){error="No rendered depth is available";return false;}
    projectionNear=std::max(0.0001f,projectionNear);projectionFar=std::max(projectionNear+0.001f,projectionFar);rangeNear=std::max(0.0f,rangeNear);rangeFar=std::max(rangeNear+0.001f,rangeFar);
    const auto pixelCount=static_cast<std::size_t>(width_)*static_cast<std::size_t>(height_);
    std::vector<float> rawDepth(pixelCount);
    glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width_,height_,GL_DEPTH_COMPONENT,GL_FLOAT,rawDepth.data());glapi::BindFramebuffer(glapi::Framebuffer,0);
    using Microsoft::WRL::ComPtr;auto* factory=static_cast<IWICImagingFactory*>(imageFactory_);ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> properties;
    if(FAILED(factory->CreateStream(&stream))||FAILED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE))){error="Could not initialize output stream for "+path.filename().string();return false;}

    if(formatBitDepth==2){
        if(FAILED(factory->CreateEncoder(GUID_ContainerFormatTiff,nullptr,&encoder))||FAILED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache))||FAILED(encoder->CreateNewFrame(&frame,&properties))||FAILED(frame->Initialize(properties.Get()))||FAILED(frame->SetSize(width_,height_))){error="Could not initialize 32-bit float TIFF encoder";return false;}
        WICPixelFormatGUID format=GUID_WICPixelFormat32bppGrayFloat;
        if(FAILED(frame->SetPixelFormat(&format))||format!=GUID_WICPixelFormat32bppGrayFloat){error="System does not support 32-bit float TIFF encoding";return false;}
        std::vector<float> floatPixels(pixelCount);
        for(int y=0;y<height_;++y)for(int x=0;x<width_;++x){
            const std::size_t srcIdx=static_cast<std::size_t>(height_-1-y)*width_+x,dstIdx=static_cast<std::size_t>(y)*width_+x;
            float output{};if(rawDepth[srcIdx]<1.0f){const double linear=foreground_depth::linearize(rawDepth[srcIdx],projectionNear,projectionFar,foregroundDrawn_&&firstPersonProjection_,firstPersonProjection_);const double normalized=std::clamp((linear-rangeNear)/(rangeFar-rangeNear),0.0,1.0);output=static_cast<float>(1.0-normalized);}
            if(invert)output=1.0f-output;floatPixels[dstIdx]=output;
        }
        const auto stride=static_cast<UINT>(width_*sizeof(float)),byteCount=static_cast<UINT>(pixelCount*sizeof(float));
        if(FAILED(frame->WritePixels(height_,stride,byteCount,reinterpret_cast<BYTE*>(floatPixels.data())))||FAILED(frame->Commit())||FAILED(encoder->Commit())){error="Could not encode 32-bit float depth image";return false;}
        return true;
    }

    if(FAILED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))||FAILED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache))||FAILED(encoder->CreateNewFrame(&frame,&properties))||FAILED(frame->Initialize(properties.Get()))||FAILED(frame->SetSize(width_,height_))){error="Could not initialize PNG encoder";return false;}
    if(formatBitDepth==1){
        WICPixelFormatGUID format=GUID_WICPixelFormat16bppGray;
        if(FAILED(frame->SetPixelFormat(&format))||format!=GUID_WICPixelFormat16bppGray){error="System does not support 16-bit PNG encoding";return false;}
        std::vector<std::uint16_t> pixels16(pixelCount);
        for(int y=0;y<height_;++y)for(int x=0;x<width_;++x){
            const std::size_t srcIdx=static_cast<std::size_t>(height_-1-y)*width_+x,dstIdx=static_cast<std::size_t>(y)*width_+x;
            float output{};if(rawDepth[srcIdx]<1.0f){const double linear=foreground_depth::linearize(rawDepth[srcIdx],projectionNear,projectionFar,foregroundDrawn_&&firstPersonProjection_,firstPersonProjection_);const double normalized=std::clamp((linear-rangeNear)/(rangeFar-rangeNear),0.0,1.0);output=static_cast<float>(1.0-normalized);}
            if(invert)output=1.0f-output;pixels16[dstIdx]=static_cast<std::uint16_t>(std::clamp(std::round(output*65535.0f),0.0f,65535.0f));
        }
        const auto stride=static_cast<UINT>(width_*sizeof(std::uint16_t)),byteCount=static_cast<UINT>(pixelCount*sizeof(std::uint16_t));
        if(FAILED(frame->WritePixels(height_,stride,byteCount,reinterpret_cast<BYTE*>(pixels16.data())))||FAILED(frame->Commit())||FAILED(encoder->Commit())){error="Could not encode 16-bit depth PNG";return false;}
        return true;
    }

    WICPixelFormatGUID format=GUID_WICPixelFormat8bppGray;
    if(FAILED(frame->SetPixelFormat(&format))||format!=GUID_WICPixelFormat8bppGray){error="System does not support 8-bit grayscale PNG encoding";return false;}
    std::vector<std::uint8_t> pixels8(pixelCount);
    for(int y=0;y<height_;++y)for(int x=0;x<width_;++x){
        const std::size_t srcIdx=static_cast<std::size_t>(height_-1-y)*width_+x,dstIdx=static_cast<std::size_t>(y)*width_+x;
        float output{};if(rawDepth[srcIdx]<1.0f){const double linear=foreground_depth::linearize(rawDepth[srcIdx],projectionNear,projectionFar,foregroundDrawn_&&firstPersonProjection_,firstPersonProjection_);const double normalized=std::clamp((linear-rangeNear)/(rangeFar-rangeNear),0.0,1.0);output=static_cast<float>(1.0-normalized);}
        if(invert)output=1.0f-output;pixels8[dstIdx]=static_cast<std::uint8_t>(std::clamp(std::round(output*255.0f),0.0f,255.0f));
    }
    const auto stride=static_cast<UINT>(width_*sizeof(std::uint8_t)),byteCount=static_cast<UINT>(pixelCount*sizeof(std::uint8_t));
    if(FAILED(frame->WritePixels(height_,stride,byteCount,pixels8.data()))||FAILED(frame->Commit())||FAILED(encoder->Commit())){error="Could not encode 8-bit depth PNG";return false;}
    return true;
#else
    (void)path;(void)formatBitDepth;(void)projectionNear;(void)projectionFar;(void)rangeNear;(void)rangeFar;(void)invert;
    error="Depth export is not implemented on this platform";return false;
#endif
}
void StageRenderer::renderMuzzleFlash3D(scene::Vec3 position, float size, float rotation, scene::Vec4 color, const scene::Mat4& viewProjection, scene::Vec3 cameraPos, bool firstPerson) {
    if (size <= 0.001f || color.w <= 0.001f) return;
    if (!billboardProgram_ || !billboardVao_ || !billboardBuffer_ || !framebuffer_) return;

    scene::Vec3 toCam = cameraPos - position;
    if (scene::length(toCam) < 0.001f) toCam = {0, 0, 1};
    else toCam = scene::normalize(toCam);

    scene::Vec3 up{0, 0, 1};
    if (std::abs(scene::dot(toCam, up)) > 0.95f) up = {0, 1, 0};
    scene::Vec3 right = scene::normalize(scene::cross(up, toCam));
    up = scene::normalize(scene::cross(toCam, right));

    const float c = std::cos(rotation), s = std::sin(rotation);
    const scene::Vec3 r = (right * c + up * s) * (size * 0.5f);
    const scene::Vec3 u = (up * c - right * s) * (size * 0.5f);

    const scene::Vec3 p0 = position - r - u;
    const scene::Vec3 p1 = position + r - u;
    const scene::Vec3 p2 = position + r + u;
    const scene::Vec3 p3 = position - r + u;

    float verts[] = {
        p0.x, p0.y, p0.z,  0.0f, 0.0f,  color.x, color.y, color.z, color.w,
        p1.x, p1.y, p1.z,  1.0f, 0.0f,  color.x, color.y, color.z, color.w,
        p2.x, p2.y, p2.z,  1.0f, 1.0f,  color.x, color.y, color.z, color.w,

        p0.x, p0.y, p0.z,  0.0f, 0.0f,  color.x, color.y, color.z, color.w,
        p2.x, p2.y, p2.z,  1.0f, 1.0f,  color.x, color.y, color.z, color.w,
        p3.x, p3.y, p3.z,  0.0f, 1.0f,  color.x, color.y, color.z, color.w,
    };

    glapi::BindFramebuffer(glapi::Framebuffer, framebuffer_);
    glViewport(0, 0, width_, height_);
    glDepthRange(0.0, firstPerson ? 0.04 : 1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glapi::UseProgram(billboardProgram_);
    glapi::Uniform2f(uniformLocation(billboardProgram_,"uForegroundClip"),firstPerson&&firstPersonProjection_?foreground_depth::nearPlane:0.f,foreground_depth::farPlane);
    glapi::UniformMatrix4fv(uniformLocation(billboardProgram_, "uViewProjection"), 1, GL_FALSE, viewProjection.data());
    glapi::Uniform1i(uniformLocation(billboardProgram_, "uTexture"), 0);
    glapi::ActiveTexture(glapi::Texture0);
    glBindTexture(GL_TEXTURE_2D, muzzleFlashTexture_);
    glapi::Uniform1i(uniformLocation(billboardProgram_, "uUseTexture"), muzzleFlashTexture_ ? 1 : 0);
    glapi::Uniform1i(uniformLocation(billboardProgram_, "uProceduralMode"), muzzleFlashTexture_ ? 0 : 2);
    // Billboard uniforms persist across effect types; a flash is not a trail.
    glapi::Uniform1f(uniformLocation(billboardProgram_, "uTrailEdgeDarkening"), 0.0f);
    glapi::Uniform1f(uniformLocation(billboardProgram_, "uTrailEdgePower"), 1.0f);
    glapi::Uniform3f(uniformLocation(billboardProgram_, "uTrailEdgeTint"), 1.0f, 1.0f, 1.0f);

    glapi::BindVertexArray(billboardVao_);
    glapi::BindBuffer(glapi::ArrayBuffer, billboardBuffer_);
    glapi::BufferData(glapi::ArrayBuffer, sizeof(verts), verts, glapi::DynamicDraw);
    glapi::DrawArrays(GL_TRIANGLES, 0, 6);
    glapi::Uniform2f(uniformLocation(billboardProgram_,"uForegroundClip"),0,0);

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDepthRange(0.0, 1.0);
    glDisable(GL_BLEND);
    glapi::BindVertexArray(0);
    glapi::UseProgram(0);
    glapi::BindFramebuffer(glapi::Framebuffer, 0);
}

void StageRenderer::renderDebugLine3D(scene::Vec3 start, scene::Vec3 end, scene::Vec4 color, const scene::Mat4& viewProjection) {
    if (!lineProgram_ || !lineVao_ || !lineBuffer_ || !framebuffer_) return;

    float verts[14] = {
        start.x, start.y, start.z, color.x, color.y, color.z, color.w,
        end.x, end.y, end.z, color.x, color.y, color.z, color.w
    };

    glapi::BindFramebuffer(glapi::Framebuffer, framebuffer_);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.0f);

    glapi::UseProgram(lineProgram_);
    glapi::UniformMatrix4fv(uniformLocation(lineProgram_, "uViewProjection"), 1, GL_FALSE, viewProjection.data());

    glapi::BindVertexArray(lineVao_);
    glapi::BindBuffer(glapi::ArrayBuffer, lineBuffer_);
    glapi::BufferData(glapi::ArrayBuffer, sizeof(verts), verts, glapi::DynamicDraw);
    glapi::DrawArrays(GL_LINES, 0, 2);

    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glapi::BindVertexArray(0);
    glapi::UseProgram(0);
    glapi::BindFramebuffer(glapi::Framebuffer, 0);
}

void StageRenderer::updateAndRenderSmokeCurve(float deltaSeconds, scene::Vec3 muzzlePos, scene::Vec3 muzzleForward, bool isEmitting, const scene::Mat4& viewProjection, scene::Vec3 cameraPos, float lifetime, float startWidth, float endWidth, float taperingExp, float blastSpeed, float riseSpeed, float dispersion, scene::Vec4 color, scene::Vec3 wind, int curveResolution,int emitter) {
    if (!framebuffer_) return;
    auto& state=smokeEmitters_[std::clamp(emitter,0,1)];auto& smokeCurvePoints_=state.points;auto& smokeTaper_=state.taper;auto& lastSmokePos_=state.lastPosition;auto& hasLastSmokePos_=state.hasLastPosition;auto& smokeSpawnTimer_=state.timer;

    deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.1f);
    if (scene::length(muzzleForward) < 0.001f) muzzleForward = {1, 0, 0};
    else muzzleForward = scene::normalize(muzzleForward);

    if (isEmitting) {
        smokeTaper_ = std::min(1.0f, smokeTaper_ + deltaSeconds * 8.0f);
        smokeSpawnTimer_ += deltaSeconds;

        const float distMoved = hasLastSmokePos_ ? scene::length(muzzlePos - lastSmokePos_) : 999.0f;
        if (!hasLastSmokePos_ || distMoved >= 0.35f || smokeSpawnTimer_ >= 0.025f) {
            smokeSpawnTimer_ = 0.0f;
            lastSmokePos_ = muzzlePos;
            hasLastSmokePos_ = true;

            scene::Vec3 tempUp = (std::abs(muzzleForward.z) < 0.95f) ? scene::Vec3{0, 0, 1} : scene::Vec3{0, 1, 0};
            scene::Vec3 side = scene::normalize(scene::cross(tempUp, muzzleForward));
            scene::Vec3 up = scene::normalize(scene::cross(muzzleForward, side));
            const float theta = (std::rand() % 6283) * 0.001f;
            const scene::Vec3 normal = side * std::cos(theta) + up * std::sin(theta);

            SmokePoint pt;
            pt.position = muzzlePos;
            pt.normal = normal;
            pt.velocity = muzzleForward * blastSpeed + scene::Vec3{0, 0, riseSpeed};
            pt.age = 0.0f;
            pt.lifetime = std::max(0.1f, lifetime);
            pt.startWidth = startWidth * smokeTaper_;
            pt.endWidth = endWidth;
            pt.taperingExp = taperingExp;
            smokeCurvePoints_.push_back(pt);
        }
    } else {
        if (smokeTaper_ > 0.0f) {
            smokeTaper_ = std::max(0.0f, smokeTaper_ - deltaSeconds * 6.0f);
            smokeSpawnTimer_ += deltaSeconds;
            if (smokeSpawnTimer_ >= 0.03f && smokeTaper_ > 0.03f) {
                smokeSpawnTimer_ = 0.0f;
                scene::Vec3 tempUp = (std::abs(muzzleForward.z) < 0.95f) ? scene::Vec3{0, 0, 1} : scene::Vec3{0, 1, 0};
                scene::Vec3 side = scene::normalize(scene::cross(tempUp, muzzleForward));
                scene::Vec3 up = scene::normalize(scene::cross(muzzleForward, side));
                const float theta = (std::rand() % 6283) * 0.001f;
                const scene::Vec3 normal = side * std::cos(theta) + up * std::sin(theta);

                SmokePoint pt;
                pt.position = muzzlePos;
                pt.normal = normal;
                pt.velocity = muzzleForward * (blastSpeed * 0.4f) + scene::Vec3{0, 0, riseSpeed};
                pt.age = 0.0f;
                pt.lifetime = std::max(0.1f, lifetime);
                pt.startWidth = startWidth * smokeTaper_;
                pt.endWidth = endWidth * smokeTaper_;
                pt.taperingExp = taperingExp;
                smokeCurvePoints_.push_back(pt);
            }
        } else {
            hasLastSmokePos_ = false;
        }
    }

    for (auto& pt : smokeCurvePoints_) {
        pt.age += deltaSeconds;
        pt.velocity = pt.velocity * std::exp(-2.2f * deltaSeconds) + (wind + scene::Vec3{0, 0, 12.0f}) * deltaSeconds;
        pt.position = pt.position + pt.velocity * deltaSeconds;
        pt.position = pt.position + pt.normal * (dispersion * deltaSeconds);
    }

    while (!smokeCurvePoints_.empty() && smokeCurvePoints_.front().age >= smokeCurvePoints_.front().lifetime) {
        smokeCurvePoints_.erase(smokeCurvePoints_.begin());
    }

    if (smokeCurvePoints_.size() > 256) {
        smokeCurvePoints_.erase(smokeCurvePoints_.begin(), smokeCurvePoints_.begin() + (smokeCurvePoints_.size() - 256));
    }

    renderSmokePoints(smokeCurvePoints_,viewProjection,cameraPos,color,curveResolution);
}

void StageRenderer::renderReplaySmoke(scene::Vec3 origin,scene::Vec3 forward,float age,std::uint32_t seed,const ReplaySmokeSettings& settings,scene::Vec4 color,const scene::Mat4& viewProjection,scene::Vec3 cameraPos,int curveResolution){
    sampleReplaySmoke(replaySmokeScratch_,origin,forward,age,seed,settings);
    renderSmokePoints(replaySmokeScratch_,viewProjection,cameraPos,color,curveResolution);
}

void StageRenderer::renderSmokePoints(const std::vector<SmokePoint>& smokeCurvePoints_,const scene::Mat4& viewProjection,scene::Vec3 cameraPos,scene::Vec4 color,int curveResolution){
    if (!framebuffer_ || smokeCurvePoints_.size() < 2) return;

    auto catmullRom = [](const scene::Vec3& p0, const scene::Vec3& p1, const scene::Vec3& p2, const scene::Vec3& p3, float t) -> scene::Vec3 {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return (p1 * 2.0f +
                (p2 - p0) * t +
                (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
                ((p1 - p2) * 3.0f + p3 - p0) * t3) * 0.5f;
    };

    const int segCount = static_cast<int>(smokeCurvePoints_.size()) - 1;
    const int kSubdivisions = std::clamp(curveResolution, 1, 8);
    const int totalSamples = segCount * kSubdivisions + 1;

    auto& samples=smokeSampleScratch_;samples.clear();
    samples.reserve(totalSamples);

    for (int seg = 0; seg < segCount; ++seg) {
        const auto& pt0 = (seg == 0) ? smokeCurvePoints_[0] : smokeCurvePoints_[seg - 1];
        const auto& pt1 = smokeCurvePoints_[seg];
        const auto& pt2 = smokeCurvePoints_[seg + 1];
        const auto& pt3 = (seg + 2 < static_cast<int>(smokeCurvePoints_.size())) ? smokeCurvePoints_[seg + 2] : smokeCurvePoints_.back();

        const int steps = (seg == segCount - 1) ? (kSubdivisions + 1) : kSubdivisions;
        for (int step = 0; step < steps; ++step) {
            const float localT = static_cast<float>(step) / static_cast<float>(kSubdivisions);
            const float overallU = (static_cast<float>(seg) + localT) / static_cast<float>(segCount);

            const scene::Vec3 pos = catmullRom(pt0.position, pt1.position, pt2.position, pt3.position, localT);
            const float nextT = std::min(1.0f, localT + 0.05f);
            const scene::Vec3 nextPos = catmullRom(pt0.position, pt1.position, pt2.position, pt3.position, nextT);
            scene::Vec3 tangent = nextPos - pos;
            if (scene::length(tangent) < 0.0001f) tangent = pt2.position - pt1.position;
            if (scene::length(tangent) < 0.0001f) tangent = {1, 0, 0};
            tangent = scene::normalize(tangent);

            scene::Vec3 toCam = cameraPos - pos;
            if (scene::length(toCam) < 0.001f) toCam = {0, 0, 1};
            else toCam = scene::normalize(toCam);

            scene::Vec3 norm = scene::cross(tangent, toCam);
            if (scene::length(norm) < 0.001f) {
                scene::Vec3 up{0, 0, 1};
                if (std::abs(scene::dot(toCam, up)) > 0.95f) up = {0, 1, 0};
                norm = scene::cross(up, toCam);
            }
            norm = scene::normalize(norm);

            const float age = pt1.age + (pt2.age - pt1.age) * localT;
            const float maxLife = pt1.lifetime + (pt2.lifetime - pt1.lifetime) * localT;
            const float normAge = std::clamp(age / std::max(0.01f, maxLife), 0.0f, 1.0f);
            const float fade = std::pow(1.0f - normAge, 1.2f);
            const float sW = pt1.startWidth + (pt2.startWidth - pt1.startWidth) * localT;
            const float eW = pt1.endWidth + (pt2.endWidth - pt1.endWidth) * localT;
            const float expVal = pt1.taperingExp + (pt2.taperingExp - pt1.taperingExp) * localT;
            const float width = (sW + (eW - sW) * std::pow(normAge, expVal)) * (1.0f + normAge * 0.6f);

            samples.push_back({pos, norm, width, color.w * fade, overallU});
        }
    }

    auto& verts=smokeVertexScratch_;verts.clear();
    verts.reserve(samples.size() * 6 * 9);

    for (std::size_t i = 0; i + 1 < samples.size(); ++i) {
        const auto& s0 = samples[i];
        const auto& s1 = samples[i + 1];
        if (s0.alpha <= 0.003f && s1.alpha <= 0.003f) continue;

        const scene::Vec3 p0L = s0.pos - s0.normal * (s0.width * 0.5f);
        const scene::Vec3 p0R = s0.pos + s0.normal * (s0.width * 0.5f);
        const scene::Vec3 p1L = s1.pos - s1.normal * (s1.width * 0.5f);
        const scene::Vec3 p1R = s1.pos + s1.normal * (s1.width * 0.5f);

        const float quadVerts[6 * 9] = {
            p0L.x, p0L.y, p0L.z,   s0.u, 0.0f,   color.x, color.y, color.z, s0.alpha,
            p1L.x, p1L.y, p1L.z,   s1.u, 0.0f,   color.x, color.y, color.z, s1.alpha,
            p1R.x, p1R.y, p1R.z,   s1.u, 1.0f,   color.x, color.y, color.z, s1.alpha,

            p0L.x, p0L.y, p0L.z,   s0.u, 0.0f,   color.x, color.y, color.z, s0.alpha,
            p1R.x, p1R.y, p1R.z,   s1.u, 1.0f,   color.x, color.y, color.z, s1.alpha,
            p0R.x, p0R.y, p0R.z,   s0.u, 1.0f,   color.x, color.y, color.z, s0.alpha,
        };
        verts.insert(verts.end(), std::begin(quadVerts), std::end(quadVerts));
    }

    if (verts.empty() || !billboardProgram_ || !billboardVao_ || !billboardBuffer_) return;

    glapi::BindFramebuffer(glapi::Framebuffer, framebuffer_);
    glViewport(0, 0, width_, height_);
    glDepthRange(0.0, 1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glapi::BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glapi::UseProgram(billboardProgram_);
    glapi::UniformMatrix4fv(uniformLocation(billboardProgram_, "uViewProjection"), 1, GL_FALSE, viewProjection.data());
    glapi::Uniform1i(uniformLocation(billboardProgram_, "uTexture"), 0);
    glapi::ActiveTexture(glapi::Texture0);
    glBindTexture(GL_TEXTURE_2D, smokeTexture_);
    glapi::Uniform1i(uniformLocation(billboardProgram_, "uUseTexture"), smokeTexture_ ? 1 : 0);
    glapi::Uniform1i(uniformLocation(billboardProgram_, "uProceduralMode"), 3);
    glapi::Uniform1f(uniformLocation(billboardProgram_, "uFeathering"), smokeFeathering_);
    // Fresh GL tint defaults to black; previous trails can also leave a dark
    // gradient here. Smoke must use its own color, independent of draw history.
    glapi::Uniform1f(uniformLocation(billboardProgram_, "uTrailEdgeDarkening"), 0.0f);
    glapi::Uniform1f(uniformLocation(billboardProgram_, "uTrailEdgePower"), 1.0f);
    glapi::Uniform3f(uniformLocation(billboardProgram_, "uTrailEdgeTint"), 1.0f, 1.0f, 1.0f);

    glapi::BindVertexArray(billboardVao_);
    glapi::BindBuffer(glapi::ArrayBuffer, billboardBuffer_);
    glapi::BufferData(glapi::ArrayBuffer, static_cast<glapi::Size>(verts.size() * sizeof(float)), verts.data(), glapi::DynamicDraw);
    glapi::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 9));

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glapi::BindVertexArray(0);
    glapi::UseProgram(0);
    glapi::BindFramebuffer(glapi::Framebuffer, 0);
}

void StageRenderer::clearSmokeCurve() {
    for(auto& state:smokeEmitters_){state.points.clear();state.taper=state.timer=0;state.hasLastPosition=false;}
}

bool StageRenderer::setImpactSurfaceTexture(const std::filesystem::path& path, std::string& error) {
    error.clear();
    const auto texture = loadTexture(path, false);
    if (!texture) {
        error = "Could not load surface impact texture " + path.filename().string();
        return false;
    }
    clearImpactSurfaceTexture();
    impactSurfaceTexture_ = texture;
    return true;
}

void StageRenderer::clearImpactSurfaceTexture() {
    if (impactSurfaceTexture_) {
        glDeleteTextures(1, &impactSurfaceTexture_);
        impactSurfaceTexture_ = 0;
    }
}

bool StageRenderer::setImpactBotTexture(const std::filesystem::path& path, std::string& error) {
    error.clear();
    const auto texture = loadTexture(path, false);
    if (!texture) {
        error = "Could not load bot impact texture " + path.filename().string();
        return false;
    }
    clearImpactBotTexture();
    impactBotTexture_ = texture;
    return true;
}

void StageRenderer::clearImpactBotTexture() {
    if (impactBotTexture_) {
        glDeleteTextures(1, &impactBotTexture_);
        impactBotTexture_ = 0;
    }
}

void StageRenderer::spawnImpact(scene::Vec3 position, scene::Vec3 normal, bool isBot, float startWidth, float endWidth, float lifetime, float blastSpeed, float riseSpeed, float dispersion, float taperingExp, float feathering, scene::Vec4 color, int planeCount, float texScale, float texOffsetX, float texOffsetY, float texRotation, float originY) {
    if (scene::length(normal) < 0.001f) normal = {0, 0, 1};
    normal = scene::normalize(normal);

    scene::Vec3 tempUp = (std::abs(normal.z) < 0.95f) ? scene::Vec3{0, 0, 1} : scene::Vec3{0, 1, 0};
    scene::Vec3 tangent = scene::normalize(scene::cross(tempUp, normal));
    scene::Vec3 bitangent = scene::normalize(scene::cross(normal, tangent));

    planeCount = std::clamp(planeCount, 2, 8);
    ImpactBurst burst;
    burst.position = position + normal * 0.15f;
    burst.normal = normal;
    burst.color = color;
    burst.age = 0.0f;
    burst.lifetime = std::max(0.05f, lifetime);
    burst.startWidth = std::max(0.1f, startWidth);
    burst.endWidth = std::max(0.1f, endWidth);
    burst.taperingExp = taperingExp;
    burst.feathering = std::clamp(feathering, 0.0f, 1.0f);
    burst.dispersion = dispersion;
    burst.isBot = isBot;
    burst.texScale = texScale;
    burst.texOffsetX = texOffsetX;
    burst.texOffsetY = texOffsetY;
    burst.texRotation = texRotation;
    burst.originY = std::clamp(originY, 0.0f, 1.0f);

    const float spreadAngle = (std::rand() % 6283) * 0.001f;
    const float spreadMagnitude = (std::rand() % 1000) * 0.001f;
    const scene::Vec3 radialDir = tangent * std::cos(spreadAngle) + bitangent * std::sin(spreadAngle);
    burst.velocity = normal * blastSpeed + radialDir * (dispersion * spreadMagnitude) + scene::Vec3{0, 0, riseSpeed};

    const float baseAngle = (std::rand() % 6283) * 0.001f;
    for (int p = 0; p < planeCount; ++p) {
        const float angle = baseAngle + (static_cast<float>(p) * scene::kPi / static_cast<float>(planeCount));
        const scene::Vec3 planeTangent = tangent * std::cos(angle) + bitangent * std::sin(angle);
        const float tilt = ((std::rand() % 30) - 15) * 0.01f;
        const scene::Vec3 planeUp = scene::normalize(normal + planeTangent * tilt);
        burst.planes.push_back({planeTangent, planeUp});
    }

    impactBursts_.push_back(std::move(burst));
    if (impactBursts_.size() > 128) {
        impactBursts_.erase(impactBursts_.begin(), impactBursts_.begin() + (impactBursts_.size() - 128));
    }
}

void StageRenderer::updateAndRenderImpacts(float deltaSeconds, const scene::Mat4& viewProjection, scene::Vec3 cameraPos) {
    (void)cameraPos;
    if (!framebuffer_ || impactBursts_.empty() || !billboardProgram_ || !billboardVao_ || !billboardBuffer_) return;

    deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.1f);
    for (auto& b : impactBursts_) {
        b.age += deltaSeconds;
        b.velocity = b.velocity * std::exp(-2.2f * deltaSeconds) + scene::Vec3{0, 0, -40.0f} * deltaSeconds;
        b.position = b.position + b.velocity * deltaSeconds;
    }

    while (!impactBursts_.empty() && impactBursts_.front().age >= impactBursts_.front().lifetime) {
        impactBursts_.erase(impactBursts_.begin());
    }
    if (impactBursts_.empty()) return;

    auto renderBatch = [&](bool forBots) {
        auto& verts=impactVertexScratch_;verts.clear();
        unsigned currentTex = forBots ? impactBotTexture_ : impactSurfaceTexture_;
        float currentFeathering = 0.8f;

        for (const auto& b : impactBursts_) {
            if (b.isBot != forBots) continue;
            currentFeathering = b.feathering;
            const float progress = std::clamp(b.age / std::max(0.01f, b.lifetime), 0.0f, 1.0f);
            const float width = b.startWidth + (b.endWidth - b.startWidth) * std::pow(progress, b.taperingExp);
            const float halfW = width * 0.5f;
            const float fade = std::pow(1.0f - progress, 1.25f);
            const float currentAlpha = b.color.w * fade;
            if (currentAlpha <= 0.003f) continue;

            // Treat hit point as origin (originY = 0.0 means base of sprite is at hit point, extending outward)
            const float y0 = -b.originY * width;
            const float y1 = (1.0f - b.originY) * width;

            const float rad = b.texRotation * (scene::kPi / 180.0f);
            const float cosR = std::cos(rad);
            const float sinR = std::sin(rad);

            auto xformUv = [&](float u, float v) -> std::pair<float, float> {
                float du = (u - 0.5f) * b.texScale;
                float dv = (v - 0.5f) * b.texScale;
                float ru = du * cosR - dv * sinR;
                float rv = du * sinR + dv * cosR;
                return { ru + 0.5f + b.texOffsetX, rv + 0.5f + b.texOffsetY };
            };

            const auto [u0, v0] = xformUv(0.0f, 0.0f);
            const auto [u1, v1] = xformUv(1.0f, 0.0f);
            const auto [u2, v2] = xformUv(1.0f, 1.0f);
            const auto [u3, v3] = xformUv(0.0f, 1.0f);

            for (const auto& plane : b.planes) {
                const scene::Vec3 r = plane.axisX * halfW;
                const scene::Vec3 uBase = plane.axisY * y0;
                const scene::Vec3 uTop = plane.axisY * y1;

                const scene::Vec3 p0 = b.position - r + uBase;
                const scene::Vec3 p1 = b.position + r + uBase;
                const scene::Vec3 p2 = b.position + r + uTop;
                const scene::Vec3 p3 = b.position - r + uTop;

                const float quadVerts[6 * 9] = {
                    p0.x, p0.y, p0.z,  u0, v0,  b.color.x, b.color.y, b.color.z, currentAlpha,
                    p1.x, p1.y, p1.z,  u1, v1,  b.color.x, b.color.y, b.color.z, currentAlpha,
                    p2.x, p2.y, p2.z,  u2, v2,  b.color.x, b.color.y, b.color.z, currentAlpha,

                    p0.x, p0.y, p0.z,  u0, v0,  b.color.x, b.color.y, b.color.z, currentAlpha,
                    p2.x, p2.y, p2.z,  u2, v2,  b.color.x, b.color.y, b.color.z, currentAlpha,
                    p3.x, p3.y, p3.z,  u3, v3,  b.color.x, b.color.y, b.color.z, currentAlpha,
                };
                verts.insert(verts.end(), std::begin(quadVerts), std::end(quadVerts));
            }
        }

        if (verts.empty()) return;

        glapi::BindFramebuffer(glapi::Framebuffer, framebuffer_);
        glViewport(0, 0, width_, height_);
        glDepthRange(0.0, 1.0);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glapi::BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        glapi::UseProgram(billboardProgram_);
        glapi::UniformMatrix4fv(uniformLocation(billboardProgram_, "uViewProjection"), 1, GL_FALSE, viewProjection.data());
        glapi::Uniform1i(uniformLocation(billboardProgram_, "uTexture"), 0);
        glapi::ActiveTexture(glapi::Texture0);
        glBindTexture(GL_TEXTURE_2D, currentTex);
        glapi::Uniform1i(uniformLocation(billboardProgram_, "uUseTexture"), currentTex ? 1 : 0);
        glapi::Uniform1i(uniformLocation(billboardProgram_, "uProceduralMode"), currentTex ? 0 : 4);
        glapi::Uniform1f(uniformLocation(billboardProgram_, "uFeathering"), currentFeathering);
        glapi::Uniform1f(uniformLocation(billboardProgram_, "uTrailEdgeDarkening"), 0.0f);
        glapi::Uniform1f(uniformLocation(billboardProgram_, "uTrailEdgePower"), 1.0f);
        glapi::Uniform3f(uniformLocation(billboardProgram_, "uTrailEdgeTint"), 1.0f, 1.0f, 1.0f);

        glapi::BindVertexArray(billboardVao_);
        glapi::BindBuffer(glapi::ArrayBuffer, billboardBuffer_);
        glapi::BufferData(glapi::ArrayBuffer, static_cast<glapi::Size>(verts.size() * sizeof(float)), verts.data(), glapi::DynamicDraw);
        glapi::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 9));

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    };

    renderBatch(false);
    renderBatch(true);
    glapi::BindVertexArray(0);
    glapi::UseProgram(0);
    glapi::BindFramebuffer(glapi::Framebuffer, 0);
}

void StageRenderer::clearImpacts() {
    impactBursts_.clear();
}

void StageRenderer::addBulletTrail(const BulletTrail& trail) {
    bulletTrails_.push_back(trail);
}

bool StageRenderer::setBulletTrailTexture(const std::filesystem::path& path, std::string& error) {
    error.clear();
    const auto texture = loadTexture(path, false);
    if (!texture) {
        error = "Could not load bullet trail texture " + path.filename().string();
        return false;
    }
    clearBulletTrailTexture();
    bulletTrailTexture_ = texture;
    return true;
}

void StageRenderer::clearBulletTrailTexture() {
    if (bulletTrailTexture_) {
        glDeleteTextures(1, &bulletTrailTexture_);
        bulletTrailTexture_ = 0;
    }
}

void StageRenderer::clearBulletTrails() {
    bulletTrails_.clear();
}

void StageRenderer::updateAndRenderBulletTrails(float deltaSeconds, const scene::Mat4& viewProjection, const scene::Vec3& cameraPos) {
    if (!framebuffer_ || bulletTrails_.empty()) return;
    deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.1f);

    for(auto& scratch:trailScratch_)scratch.clear();
    auto& ribbonEmissiveVerts=trailScratch_[0];auto& ribbonDiffuseVerts=trailScratch_[1];
    auto& spriteEmissiveVerts=trailScratch_[2];auto& spriteDiffuseVerts=trailScratch_[3];

    for (auto it = bulletTrails_.begin(); it != bulletTrails_.end(); ) {
        auto& trail = *it;
        if (trail.type == BulletTrail::Type::Sniper) {
            trail.age += deltaSeconds;
            if (trail.age >= trail.lifetime) {
                it = bulletTrails_.erase(it);
                continue;
            }
            const float normAge = std::clamp(trail.age / std::max(0.001f, trail.lifetime), 0.0f, 1.0f);
            const float fade = std::pow(1.0f - normAge, 1.5f);
            if (fade <= 0.005f) {
                ++it;
                continue;
            }

            const scene::Vec3 p0 = trail.start;
            const scene::Vec3 p1 = trail.end;
            scene::Vec3 dir = p1 - p0;
            const float len = scene::length(dir);
            if (len < 0.01f) {
                ++it;
                continue;
            }
            dir = dir / len;

            scene::Vec3 toCam = cameraPos - (p0 + p1) * 0.5f;
            if (scene::length(toCam) < 0.001f) toCam = {0, 0, 1};
            else toCam = scene::normalize(toCam);

            scene::Vec3 normal = scene::cross(dir, toCam);
            if (scene::length(normal) < 0.001f) {
                scene::Vec3 up{0, 0, 1};
                if (std::abs(scene::dot(toCam, up)) > 0.95f) up = {0, 1, 0};
                normal = scene::cross(dir, up);
            }
            normal = scene::normalize(normal);

            const float hw0 = trail.width * 0.5f * std::max(0.0f, trail.curveEnabled?trail.taperCurve.sample(0):trail.startTaper);
            const float hw1 = trail.width * 0.5f * std::max(0.0f, trail.endTaper);
            const scene::Vec3 p0L = p0 - normal * hw0;
            const scene::Vec3 p0R = p0 + normal * hw0;
            const scene::Vec3 p1L = p1 - normal * hw1;
            const scene::Vec3 p1R = p1 + normal * hw1;

            scene::Vec3 outColor = trail.color;
            if (trail.emissiveEnabled) {
                outColor = trail.color * trail.emissiveIntensity;
            } else {
                const float ndl = std::max(0.20f, scene::dot(normal, scene::Vec3{0, 0, 0} - sunDirection_));
                const scene::Vec3 sunLight = sunEnabled_ ? (sunColor_ * (sunIntensity_ * ndl)) : scene::Vec3{0, 0, 0};
                const scene::Vec3 totalLight = ambientColor_ * sunAmbient_ + sunLight;
                outColor = {trail.color.x * totalLight.x, trail.color.y * totalLight.y, trail.color.z * totalLight.z};
            }
            const float alpha = fade;

            const float quadVerts[6 * 9] = {
                p0L.x, p0L.y, p0L.z,   0.0f, 0.0f,   outColor.x, outColor.y, outColor.z, alpha,
                p1L.x, p1L.y, p1L.z,   1.0f, 0.0f,   outColor.x, outColor.y, outColor.z, alpha,
                p1R.x, p1R.y, p1R.z,   1.0f, 1.0f,   outColor.x, outColor.y, outColor.z, alpha,

                p0L.x, p0L.y, p0L.z,   0.0f, 0.0f,   outColor.x, outColor.y, outColor.z, alpha,
                p1R.x, p1R.y, p1R.z,   1.0f, 1.0f,   outColor.x, outColor.y, outColor.z, alpha,
                p0R.x, p0R.y, p0R.z,   0.0f, 1.0f,   outColor.x, outColor.y, outColor.z, alpha,
            };

            auto& targetVerts = trail.useSprite ? (trail.emissiveEnabled ? spriteEmissiveVerts : spriteDiffuseVerts)
                                                : (trail.emissiveEnabled ? ribbonEmissiveVerts : ribbonDiffuseVerts);
            if(!trail.curveEnabled)targetVerts.insert(targetVerts.end(), std::begin(quadVerts), std::end(quadVerts));
            else {
                const scene::Vec3 begin=trail.type==BulletTrail::Type::Sniper?trail.start:trail.start;
                const scene::Vec3 finish=trail.type==BulletTrail::Type::Sniper?trail.end:trail.end;
                constexpr int segments=48;
                for(int segment=0;segment<segments;++segment){
                    const float u0=float(segment)/segments,u1=float(segment+1)/segments;
                    const auto q0=begin+(finish-begin)*u0,q1=begin+(finish-begin)*u1;
                    const float w0=trail.width*.5f*std::max(0.f,trail.taperCurve.sample(u0)),w1=trail.width*.5f*std::max(0.f,trail.taperCurve.sample(u1));
                    const scene::Vec3 corners[]{q0-normal*w0,q1-normal*w1,q1+normal*w1,q0+normal*w0};
                    constexpr int indices[]{0,1,2,0,2,3};
                    const float uv[][2]{{u0,0},{u1,0},{u1,1},{u0,1}};
                    for(int index:indices){const auto& p=corners[index];targetVerts.insert(targetVerts.end(),{p.x,p.y,p.z,uv[index][0],uv[index][1],outColor.x,outColor.y,outColor.z,alpha});}
                }
            }
            // A ribbon seen directly along the shot axis has zero projected area.
            // Give its near end a camera-facing cross section so stationary shots
            // remain visible too; fade it away as the ribbon becomes side-on.
            const float axial = std::abs(scene::dot(dir, toCam));
            const float capFade = std::clamp((axial - 0.98f) / 0.02f, 0.0f, 1.0f);
            if (capFade > 0.0f && hw0 > 0.0f) {
                const scene::Vec3 capUp = scene::normalize(scene::cross(toCam, normal));
                const scene::Vec3 corners[]{p0-normal*hw0-capUp*hw0,p0+normal*hw0-capUp*hw0,
                    p0+normal*hw0+capUp*hw0,p0-normal*hw0+capUp*hw0};
                constexpr int indices[]{0,1,2,0,2,3};
                constexpr float uv[][2]{{0,0},{1,0},{1,1},{0,1}};
                auto& capVerts = trail.emissiveEnabled ? spriteEmissiveVerts : spriteDiffuseVerts;
                for (const int index : indices) {
                    const auto& p = corners[index];
                    capVerts.insert(capVerts.end(),{p.x,p.y,p.z,uv[index][0],uv[index][1],
                        outColor.x,outColor.y,outColor.z,alpha*capFade});
                }
            }
            ++it;
        } else {
            trail.currentDist += trail.speed * deltaSeconds;
            const float tailDist = std::max(0.0f, trail.currentDist - trail.length);
            const float headDist = std::min(trail.distance, trail.currentDist);

            if (tailDist >= trail.distance || headDist <= tailDist) {
                it = bulletTrails_.erase(it);
                continue;
            }

            const scene::Vec3 pTail = trail.start + trail.dir * tailDist;
            const scene::Vec3 pHead = trail.start + trail.dir * headDist;

            scene::Vec3 toCam = cameraPos - (pTail + pHead) * 0.5f;
            if (scene::length(toCam) < 0.001f) toCam = {0, 0, 1};
            else toCam = scene::normalize(toCam);

            scene::Vec3 normal = scene::cross(trail.dir, toCam);
            if (scene::length(normal) < 0.001f) {
                scene::Vec3 up{0, 0, 1};
                if (std::abs(scene::dot(toCam, up)) > 0.95f) up = {0, 1, 0};
                normal = scene::cross(trail.dir, up);
            }
            normal = scene::normalize(normal);

            const float hw0 = trail.width * 0.5f * std::max(0.0f, trail.curveEnabled?trail.taperCurve.sample(0):trail.startTaper);
            const float hw1 = trail.width * 0.5f * std::max(0.0f, trail.endTaper);
            const scene::Vec3 p0L = pTail - normal * hw0;
            const scene::Vec3 p0R = pTail + normal * hw0;
            const scene::Vec3 p1L = pHead - normal * hw1;
            const scene::Vec3 p1R = pHead + normal * hw1;

            scene::Vec3 outColor = trail.color;
            if (trail.emissiveEnabled) {
                outColor = trail.color * trail.emissiveIntensity;
            } else {
                const float ndl = std::max(0.20f, scene::dot(normal, scene::Vec3{0, 0, 0} - sunDirection_));
                const scene::Vec3 sunLight = sunEnabled_ ? (sunColor_ * (sunIntensity_ * ndl)) : scene::Vec3{0, 0, 0};
                const scene::Vec3 totalLight = ambientColor_ * sunAmbient_ + sunLight;
                outColor = {trail.color.x * totalLight.x, trail.color.y * totalLight.y, trail.color.z * totalLight.z};
            }
            const float alpha = 1.0f;

            const float quadVerts[6 * 9] = {
                p0L.x, p0L.y, p0L.z,   0.0f, 0.0f,   outColor.x, outColor.y, outColor.z, alpha,
                p1L.x, p1L.y, p1L.z,   1.0f, 0.0f,   outColor.x, outColor.y, outColor.z, alpha,
                p1R.x, p1R.y, p1R.z,   1.0f, 1.0f,   outColor.x, outColor.y, outColor.z, alpha,

                p0L.x, p0L.y, p0L.z,   0.0f, 0.0f,   outColor.x, outColor.y, outColor.z, alpha,
                p1R.x, p1R.y, p1R.z,   1.0f, 1.0f,   outColor.x, outColor.y, outColor.z, alpha,
                p0R.x, p0R.y, p0R.z,   0.0f, 1.0f,   outColor.x, outColor.y, outColor.z, alpha,
            };

            auto& targetVerts = trail.useSprite ? (trail.emissiveEnabled ? spriteEmissiveVerts : spriteDiffuseVerts)
                                                : (trail.emissiveEnabled ? ribbonEmissiveVerts : ribbonDiffuseVerts);
            if(!trail.curveEnabled)targetVerts.insert(targetVerts.end(), std::begin(quadVerts), std::end(quadVerts));
            else {
                const scene::Vec3 begin=trail.type==BulletTrail::Type::Sniper?trail.start:pTail;
                const scene::Vec3 finish=trail.type==BulletTrail::Type::Sniper?trail.end:pHead;
                constexpr int segments=48;
                for(int segment=0;segment<segments;++segment){
                    const float u0=float(segment)/segments,u1=float(segment+1)/segments;
                    const auto q0=begin+(finish-begin)*u0,q1=begin+(finish-begin)*u1;
                    const float w0=trail.width*.5f*std::max(0.f,trail.taperCurve.sample(u0)),w1=trail.width*.5f*std::max(0.f,trail.taperCurve.sample(u1));
                    const scene::Vec3 corners[]{q0-normal*w0,q1-normal*w1,q1+normal*w1,q0+normal*w0};
                    constexpr int indices[]{0,1,2,0,2,3};
                    const float uv[][2]{{u0,0},{u1,0},{u1,1},{u0,1}};
                    for(int index:indices){const auto& p=corners[index];targetVerts.insert(targetVerts.end(),{p.x,p.y,p.z,uv[index][0],uv[index][1],outColor.x,outColor.y,outColor.z,alpha});}
                }
            }
            ++it;
        }
    }

    if (ribbonEmissiveVerts.empty() && ribbonDiffuseVerts.empty() && spriteEmissiveVerts.empty() && spriteDiffuseVerts.empty()) return;
    if (!billboardProgram_ || !billboardVao_ || !billboardBuffer_) return;

    glapi::BindFramebuffer(glapi::Framebuffer, framebuffer_);
    glViewport(0, 0, width_, height_);
    glDepthRange(0.0, 1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    glapi::UseProgram(billboardProgram_);
    glapi::UniformMatrix4fv(uniformLocation(billboardProgram_, "uViewProjection"), 1, GL_FALSE, viewProjection.data());
    glapi::Uniform1i(uniformLocation(billboardProgram_, "uTexture"), 0);

    auto drawBatch = [&](const std::vector<float>& batchVerts, bool emissive, bool sprite) {
        if (batchVerts.empty()) return;
        if (emissive) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glapi::Uniform1f(uniformLocation(billboardProgram_, "uTrailEdgeDarkening"), trailEdgeDarkening_);
            glapi::Uniform1f(uniformLocation(billboardProgram_, "uTrailEdgePower"), trailEdgePower_);
            glapi::Uniform3f(uniformLocation(billboardProgram_, "uTrailEdgeTint"), trailEdgeTint_.x, trailEdgeTint_.y, trailEdgeTint_.z);
        } else {
            glapi::BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glapi::Uniform1f(uniformLocation(billboardProgram_, "uTrailEdgeDarkening"), 0.0f);
            glapi::Uniform1f(uniformLocation(billboardProgram_, "uTrailEdgePower"), 1.0f);
            glapi::Uniform3f(uniformLocation(billboardProgram_, "uTrailEdgeTint"), 1.0f, 1.0f, 1.0f);
        }

        if (sprite) {
            if (bulletTrailTexture_ != 0) {
                glapi::ActiveTexture(glapi::Texture0);
                glBindTexture(GL_TEXTURE_2D, bulletTrailTexture_);
                glapi::Uniform1i(uniformLocation(billboardProgram_, "uUseTexture"), 1);
                glapi::Uniform1i(uniformLocation(billboardProgram_, "uProceduralMode"), 0);
            } else {
                glapi::Uniform1i(uniformLocation(billboardProgram_, "uUseTexture"), 0);
                glapi::Uniform1i(uniformLocation(billboardProgram_, "uProceduralMode"), 1);
            }
            glapi::Uniform1f(uniformLocation(billboardProgram_, "uFeathering"), 0.5f);
        } else {
            glapi::Uniform1i(uniformLocation(billboardProgram_, "uUseTexture"), 0);
            glapi::Uniform1i(uniformLocation(billboardProgram_, "uProceduralMode"), 3);
            glapi::Uniform1f(uniformLocation(billboardProgram_, "uFeathering"), 0.35f);
        }

        glapi::BindVertexArray(billboardVao_);
        glapi::BindBuffer(glapi::ArrayBuffer, billboardBuffer_);
        glapi::BufferData(glapi::ArrayBuffer, static_cast<glapi::Size>(batchVerts.size() * sizeof(float)), batchVerts.data(), glapi::DynamicDraw);
        glapi::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batchVerts.size() / 9));
    };

    drawBatch(ribbonDiffuseVerts, false, false);
    drawBatch(spriteDiffuseVerts, false, true);
    drawBatch(ribbonEmissiveVerts, true, false);
    drawBatch(spriteEmissiveVerts, true, true);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glapi::BindVertexArray(0);
    glapi::UseProgram(0);
    glapi::BindFramebuffer(glapi::Framebuffer, 0);
}

StageRenderer::RenderStats StageRenderer::renderStats(bool queryDriver) const noexcept {
    RenderStats s;
    s.mapMeshesTotal = static_cast<int>(mapMeshCount_);
    s.mapOpaqueMeshes = static_cast<int>(mapOpaqueMeshes_.size());
    s.mapDecalMeshes = static_cast<int>(mapDecalMeshes_.size());
    s.mapTransparentMeshes = static_cast<int>(mapTransparentMeshes_.size());
    s.mapAdditiveMeshes = static_cast<int>(mapAdditiveMeshes_.size());
    s.mapShadowCasters = static_cast<int>(mapShadowCasterMeshes_.size());
    s.lastVisibleMapMeshes = lastVisibleMapMeshes_;
    s.lastShadowCasterDraws = lastShadowCasterDraws_;
    s.lastFarShadowCasterDraws = lastFarShadowCasterDraws_;
    s.lastTotalDrawCalls = lastTotalDrawCalls_;
    s.materialRequests=lastMaterialRequests_;s.materialUploads=lastMaterialUploads_;s.emissionTextureBinds=lastEmissionTextureBinds_;
    s.poseRequests=lastPoseRequests_;s.poseUploads=lastPoseUploads_;
    s.loadedTextureCount = static_cast<int>(sceneTextureCache_.size());
    s.estimatedVramBytes = estimatedVramBytes_;
    s.mapTextureResolution = mapTextureResolution_;
    s.hardwareCompressionEnabled = hardwareTextureCompression_;
    if(!queryDriver)return s; // Diagnostic sampling must not introduce driver synchronization.
    if(!driverPoll_.due(DriverPollInterval::Clock::now())){s.vramTotalMb=driverTotalMb_;s.vramFreeMb=driverFreeMb_;return s;}
    GLint totalKb = 0, availKb = 0;
    glGetIntegerv(0x9047, &totalKb);
    glGetIntegerv(0x9049, &availKb);
    if (glGetError() == GL_NO_ERROR && totalKb > 0) {
        s.vramTotalMb = totalKb / 1024;
        s.vramFreeMb = availKb / 1024;
    }
    driverTotalMb_=s.vramTotalMb;driverFreeMb_=s.vramFreeMb;
    return s;
}

} // namespace render
