#pragma once
#include "scene/Math3D.h"
#include <vector>
#include <stdexcept>
namespace scene::volume_fit {
inline Vec3 vector(const Mat4&m,Vec3 p){return {m.v[0]*p.x+m.v[4]*p.y+m.v[8]*p.z,m.v[1]*p.x+m.v[5]*p.y+m.v[9]*p.z,m.v[2]*p.x+m.v[6]*p.y+m.v[10]*p.z};}
inline Mat4 transpose3(const Mat4&m){auto r=Mat4::identity();for(int c=0;c<3;++c)for(int row=0;row<3;++row)r.v[c*4+row]=m.v[row*4+c];return r;}
inline float qdot(Quat a,Quat b){return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;}
inline Quat mul(Quat q,float w){return {q.x*w,q.y*w,q.z*w,q.w*w};}
inline Quat add(Quat a,Quat b){return {a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w};}
struct Bone {Quat real{},dual{0,0,0,0};Mat4 stretch=Mat4::identity();Vec3 pivot{};};
inline Bone prepare(const Mat4& deformation,Vec3 pivot){
 // Polar decomposition separates rigid rotation from anisotropic fitting.
 auto r=deformation;r.v[12]=r.v[13]=r.v[14]=0;
 for(int step=0;step<16;++step){const auto inverseTranspose=transpose3(inverseAffine(r));float error=0;for(int c=0;c<3;++c)for(int row=0;row<3;++row){const int k=c*4+row;float value=.5f*(r.v[k]+inverseTranspose.v[k]);error=std::max(error,std::abs(value-r.v[k]));r.v[k]=value;}if(error<1e-6f)break;}
 Vec3 unused,scale;Quat q;decomposeAffine(r,unused,q,scale);q=normalize(q);r=rotation(q);
 Bone result;result.real=q;result.pivot=pivot;result.stretch=transpose3(r)*deformation;result.stretch.v[12]=result.stretch.v[13]=result.stretch.v[14]=0;
 const auto t=transformPoint(deformation,pivot)-vector(r,pivot);result.dual=mul(multiply({t.x,t.y,t.z,0},q),.5f);return result;
}
template<class Indices,class Weights> Vec3 position(const std::vector<Bone>& fits,Vec3 p,const Indices& ids,const Weights& weights,Mat4* differential=nullptr){
 Quat real{0,0,0,0},dual{0,0,0,0},reference{};bool first=true;Vec3 stretched{};float total=0;
 Mat4 stretchSum{};
 for(int k=0;k<4;++k)if(weights[k]>0){if(ids[k]>=fits.size())throw std::runtime_error("Invalid fitted skin joint");const auto&f=fits[ids[k]];if(first){reference=f.real;first=false;}const float w=weights[k],sign=qdot(reference,f.real)<0?-1.f:1.f;real=add(real,mul(f.real,w*sign));dual=add(dual,mul(f.dual,w*sign));stretched+=(f.pivot+vector(f.stretch,p-f.pivot))*w;total+=w;if(differential)for(int c=0;c<3;++c)for(int row=0;row<3;++row)stretchSum.v[c*4+row]+=f.stretch.v[c*4+row]*w;}
 if(total<.99f||total>1.01f)throw std::runtime_error("Invalid fitted skin weights");const float norm=std::sqrt(qdot(real,real));if(norm<1e-6f)throw std::runtime_error("Degenerate fitted skin rotation");real=mul(real,1/norm);dual=mul(dual,1/norm);dual=add(dual,mul(real,-qdot(real,dual)));const auto t=mul(multiply(dual,{-real.x,-real.y,-real.z,real.w}),2.f);
 if(differential){for(float&v:stretchSum.v)v/=total;stretchSum.v[15]=1;*differential=rotation(real)*stretchSum;}
 return vector(rotation(real),stretched/total)+Vec3{t.x,t.y,t.z};
}
}
