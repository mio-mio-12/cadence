#pragma once
#include "scene/CastScene.h"
#include <optional>

namespace scene {
// Surface-contact residuals from the two user-reviewed v157 grips. Expressed
// in the native PB wrist frame AFTER removing the ten-landmark rigid fit.
// These are grip-class calibrations, not world offsets or weapon-name patches.
// Keep separate from the solver: bone centers alone cannot locate glove skin.
inline Mat4 pointBlankGripSurfaceCalibration(bool pistol){
 Mat4 m;
 if(pistol)m.v={.999983526f,-.005516260f,.001594401f,0,.005243733f,.990435565f,.137876601f,0,-.002339718f,-.137865938f,.990448111f,0,-.683764498f,.239890158f,.269570971f,1};
 else m.v={.999840069f,-.017875562f,-.000597921f,0,.017879682f,.999809988f,.007765422f,0,.000459006f,-.007774867f,.999969663f,0,-.605198949f,1.719000902f,.698530454f,1};
 return m;
}
// Rigid least-squares fit. Never scales the weapon or changes skin weights.
// Horn's symmetric quaternion system is diagonalized rather than using power
// iteration (whose largest-magnitude eigenvalue can be the wrong eigenpair).
inline std::optional<Mat4> fitGripLandmarks(const std::vector<Vec3>& from,const std::vector<Vec3>& to){
 if(from.size()!=to.size()||from.size()<4)return {};
 Vec3 a{},b{};for(size_t i=0;i<from.size();++i){a+=from[i];b+=to[i];}a=a/float(from.size());b=b/float(to.size());
 double s[3][3]{};float spread=0,area=0;
 for(size_t i=0;i<from.size();++i){auto x=from[i]-a,y=to[i]-b;spread+=dot(x,x);for(auto p:from)area=std::max(area,length(cross(x,p-a)));double xv[]{x.x,x.y,x.z},yv[]{y.x,y.y,y.z};for(int j=0;j<3;++j)for(int k=0;k<3;++k)s[j][k]+=xv[j]*yv[k];}
 if(!std::isfinite(spread)||spread<.001f||area<.001f)return {};
 double n[4][4]{{s[0][0]+s[1][1]+s[2][2],s[1][2]-s[2][1],s[2][0]-s[0][2],s[0][1]-s[1][0]},
 {s[1][2]-s[2][1],s[0][0]-s[1][1]-s[2][2],s[0][1]+s[1][0],s[0][2]+s[2][0]},
 {s[2][0]-s[0][2],s[0][1]+s[1][0],-s[0][0]+s[1][1]-s[2][2],s[1][2]+s[2][1]},
 {s[0][1]-s[1][0],s[0][2]+s[2][0],s[1][2]+s[2][1],-s[0][0]-s[1][1]+s[2][2]}};
 double v[4][4]{};for(int i=0;i<4;++i)v[i][i]=1;
 for(int iter=0;iter<64;++iter){int p=0,q=1;for(int j=0;j<4;++j)for(int k=j+1;k<4;++k)if(std::abs(n[j][k])>std::abs(n[p][q])){p=j;q=k;}if(std::abs(n[p][q])<1e-12)break;
 const double angle=.5*std::atan2(2*n[p][q],n[q][q]-n[p][p]),c=std::cos(angle),sn=std::sin(angle);
 const double pp=n[p][p],qq=n[q][q],pq=n[p][q];for(int k=0;k<4;++k)if(k!=p&&k!=q){double kp=n[k][p],kq=n[k][q];n[k][p]=n[p][k]=c*kp-sn*kq;n[k][q]=n[q][k]=sn*kp+c*kq;}
 n[p][p]=c*c*pp-2*c*sn*pq+sn*sn*qq;n[q][q]=sn*sn*pp+2*c*sn*pq+c*c*qq;n[p][q]=n[q][p]=0;
 for(int k=0;k<4;++k){double kp=v[k][p],kq=v[k][q];v[k][p]=c*kp-sn*kq;v[k][q]=sn*kp+c*kq;}}
 int best=0;for(int i=1;i<4;++i)if(n[i][i]>n[best][best])best=i;
 auto m=trs({},normalize(Quat{float(v[1][best]),float(v[2][best]),float(v[3][best]),float(v[0][best])}),{1,1,1});auto pos=b-transformPoint(m,a);m.v[12]=pos.x;m.v[13]=pos.y;m.v[14]=pos.z;
 for(float f:m.v)if(!std::isfinite(f))return {};return m;
}
}
