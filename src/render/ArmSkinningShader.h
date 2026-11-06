#pragma once
#include <string>
namespace render {
// Shared by beauty and shadow shaders. Unmarked and non-rigid skins retain LBS.
inline std::string withArmSkinning(std::string source){
 const std::string code=R"GLSL(
layout(location=6) in float aArmSkinning;
vec4 armQMul(vec4 a,vec4 b){return vec4(a.w*b.xyz+b.w*a.xyz+cross(a.xyz,b.xyz),a.w*b.w-dot(a.xyz,b.xyz));}
vec4 armQuat(mat3 m){
 float tr=m[0][0]+m[1][1]+m[2][2];vec4 q;
 if(tr>0.0){float s=sqrt(tr+1.0)*2.0;q=vec4((m[1][2]-m[2][1])/s,(m[2][0]-m[0][2])/s,(m[0][1]-m[1][0])/s,.25*s);}
 else if(m[0][0]>m[1][1]&&m[0][0]>m[2][2]){float s=sqrt(1.0+m[0][0]-m[1][1]-m[2][2])*2.0;q=vec4(.25*s,(m[1][0]+m[0][1])/s,(m[2][0]+m[0][2])/s,(m[1][2]-m[2][1])/s);}
 else if(m[1][1]>m[2][2]){float s=sqrt(1.0+m[1][1]-m[0][0]-m[2][2])*2.0;q=vec4((m[1][0]+m[0][1])/s,.25*s,(m[2][1]+m[1][2])/s,(m[2][0]-m[0][2])/s);}
 else{float s=sqrt(1.0+m[2][2]-m[0][0]-m[1][1])*2.0;q=vec4((m[2][0]+m[0][2])/s,(m[2][1]+m[1][2])/s,.25*s,(m[0][1]-m[1][0])/s);}
 return normalize(q);
}
vec3 armRotate(vec4 q,vec3 p){return p+2.0*cross(q.xyz,cross(q.xyz,p)+q.w*p);}
mat4 armSkin(mat4 linear){
 if(!uSkinned||aArmSkinning<.5)return linear;
 vec4 real=vec4(0),dual=vec4(0),reference=vec4(0);bool first=true;float total=0.0;
 for(int i=0;i<4;++i){float w=aWeights[i];if(w<=0.0)continue;mat4 m=boneMatrix(aBones[i]);mat3 r=mat3(m);
  // Keep scale, shear, reflections and malformed matrices on the established path.
  if(!(abs(dot(r[0],r[0])-1.0)<.002&&abs(dot(r[1],r[1])-1.0)<.002&&abs(dot(r[2],r[2])-1.0)<.002&&abs(dot(r[0],r[1]))<.002&&abs(dot(r[0],r[2]))<.002&&abs(dot(r[1],r[2]))<.002&&determinant(r)>.998))return linear;
  vec4 q=armQuat(r);if(first){reference=q;first=false;}float signQ=dot(reference,q)<0.0?-1.0:1.0;
  real+=q*(w*signQ);dual+=.5*armQMul(vec4(m[3].xyz,0),q)*(w*signQ);total+=w;
 }
 float norm=length(real);if(!(norm>1e-6)||abs(total-1.0)>.001)return linear;
 real/=norm;dual/=norm;dual-=real*dot(real,dual);
 vec3 t=2.0*armQMul(dual,vec4(-real.xyz,real.w)).xyz;
 return mat4(vec4(armRotate(real,vec3(1,0,0)),0),vec4(armRotate(real,vec3(0,1,0)),0),vec4(armRotate(real,vec3(0,0,1)),0),vec4(t,1));
}
)GLSL";
 const auto main=source.find("void main()");source.insert(main,code);
 const auto visible=source.find("vVisible=uSkinned?");source.insert(visible,"skin=armSkin(skin);");
 return source;
}
}
