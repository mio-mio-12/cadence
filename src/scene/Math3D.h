#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace scene {

constexpr float kPi = 3.14159265358979323846f;

struct Vec2 { float x{}, y{}; };
struct Vec3 { float x{}, y{}, z{}; };
struct Vec4 { float x{}, y{}, z{}, w{1.0f}; };
using Quat = Vec4;

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3& operator+=(Vec3& a, Vec3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
inline Vec3& operator-=(Vec3& a, Vec3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator/(Vec3 a, float s) { return a * (1.0f / s); }
inline float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(Vec3 v) { const float l = length(v); return l > 1e-8f ? v / l : Vec3{}; }
inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }

inline Quat normalize(Quat q) {
    const float l = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    return l > 1e-8f ? Quat{q.x/l, q.y/l, q.z/l, q.w/l} : Quat{};
}
inline Quat multiply(Quat a, Quat b) {
    return {a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
            a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
            a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
            a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z};
}
inline Quat fromAxisAngle(Vec3 axis, float angle) {
    const Vec3 norm = normalize(axis);
    const float half = angle * 0.5f;
    const float s = std::sin(half);
    return normalize(Quat{norm.x * s, norm.y * s, norm.z * s, std::cos(half)});
}
inline Quat fromEulerRadians(Vec3 euler) {
    const float cx=std::cos(euler.x*0.5f),sx=std::sin(euler.x*0.5f);
    const float cy=std::cos(euler.y*0.5f),sy=std::sin(euler.y*0.5f);
    const float cz=std::cos(euler.z*0.5f),sz=std::sin(euler.z*0.5f);
    return normalize(Quat{sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz});
}
inline Quat slerp(Quat a, Quat b, float t) {
    a = normalize(a); b = normalize(b);
    float cosine = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    if (cosine < 0.0f) { b = {-b.x,-b.y,-b.z,-b.w}; cosine = -cosine; }
    if (cosine > 0.9995f) return normalize(Quat{a.x+t*(b.x-a.x), a.y+t*(b.y-a.y), a.z+t*(b.z-a.z), a.w+t*(b.w-a.w)});
    const float theta = std::acos(std::clamp(cosine, -1.0f, 1.0f));
    const float s = std::sin(theta);
    const float wa = std::sin((1.0f-t)*theta)/s, wb = std::sin(t*theta)/s;
    return {a.x*wa+b.x*wb, a.y*wa+b.y*wb, a.z*wa+b.z*wb, a.w*wa+b.w*wb};
}

struct Mat4 {
    std::array<float, 16> v{};
    static Mat4 identity() { Mat4 m; m.v = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; return m; }
    const float* data() const { return v.data(); }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c=0;c<4;++c) for (int row=0;row<4;++row)
        for (int k=0;k<4;++k) r.v[c*4+row] += a.v[k*4+row] * b.v[c*4+k];
    return r;
}

inline Mat4 translation(Vec3 p) { auto m=Mat4::identity(); m.v[12]=p.x; m.v[13]=p.y; m.v[14]=p.z; return m; }
inline Mat4 scale(Vec3 s) { auto m=Mat4::identity(); m.v[0]=s.x; m.v[5]=s.y; m.v[10]=s.z; return m; }
inline Mat4 rotation(Quat q) {
    q = normalize(q);
    const float xx=q.x*q.x, yy=q.y*q.y, zz=q.z*q.z, xy=q.x*q.y, xz=q.x*q.z, yz=q.y*q.z;
    const float wx=q.w*q.x, wy=q.w*q.y, wz=q.w*q.z;
    Mat4 m=Mat4::identity();
    m.v[0]=1-2*(yy+zz); m.v[1]=2*(xy+wz); m.v[2]=2*(xz-wy);
    m.v[4]=2*(xy-wz); m.v[5]=1-2*(xx+zz); m.v[6]=2*(yz+wx);
    m.v[8]=2*(xz+wy); m.v[9]=2*(yz-wx); m.v[10]=1-2*(xx+yy);
    return m;
}
inline Mat4 trs(Vec3 p, Quat q, Vec3 s) { return translation(p) * rotation(q) * scale(s); }
inline Vec3 transformPoint(const Mat4& m, Vec3 p) {
    return {m.v[0]*p.x+m.v[4]*p.y+m.v[8]*p.z+m.v[12],
            m.v[1]*p.x+m.v[5]*p.y+m.v[9]*p.z+m.v[13],
            m.v[2]*p.x+m.v[6]*p.y+m.v[10]*p.z+m.v[14]};
}
inline Mat4 inverseAffine(const Mat4& m) {
    const float a=m.v[0], b=m.v[4], c=m.v[8], d=m.v[1], e=m.v[5], f=m.v[9], g=m.v[2], h=m.v[6], i=m.v[10];
    const float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if (std::abs(det)<1e-10f) return Mat4::identity();
    const float id=1.0f/det;
    Mat4 r=Mat4::identity();
    r.v[0]=(e*i-f*h)*id; r.v[4]=(c*h-b*i)*id; r.v[8]=(b*f-c*e)*id;
    r.v[1]=(f*g-d*i)*id; r.v[5]=(a*i-c*g)*id; r.v[9]=(c*d-a*f)*id;
    r.v[2]=(d*h-e*g)*id; r.v[6]=(b*g-a*h)*id; r.v[10]=(a*e-b*d)*id;
    const Vec3 t{m.v[12],m.v[13],m.v[14]};
    const Vec3 it{-(r.v[0]*t.x+r.v[4]*t.y+r.v[8]*t.z),
                  -(r.v[1]*t.x+r.v[5]*t.y+r.v[9]*t.z),
                  -(r.v[2]*t.x+r.v[6]*t.y+r.v[10]*t.z)};
    r.v[12]=it.x; r.v[13]=it.y; r.v[14]=it.z;
    return r;
}
inline void decomposeAffine(const Mat4& m, Vec3& position, Quat& orientation, Vec3& scaling) {
    position={m.v[12],m.v[13],m.v[14]};
    scaling={length({m.v[0],m.v[1],m.v[2]}),length({m.v[4],m.v[5],m.v[6]}),length({m.v[8],m.v[9],m.v[10]})};
    const float r00=m.v[0]/std::max(scaling.x,1e-8f),r01=m.v[4]/std::max(scaling.y,1e-8f),r02=m.v[8]/std::max(scaling.z,1e-8f);
    const float r10=m.v[1]/std::max(scaling.x,1e-8f),r11=m.v[5]/std::max(scaling.y,1e-8f),r12=m.v[9]/std::max(scaling.z,1e-8f);
    const float r20=m.v[2]/std::max(scaling.x,1e-8f),r21=m.v[6]/std::max(scaling.y,1e-8f),r22=m.v[10]/std::max(scaling.z,1e-8f);
    const float trace=r00+r11+r22;
    if(trace>0){const float s=std::sqrt(trace+1.0f)*2;orientation={(r21-r12)/s,(r02-r20)/s,(r10-r01)/s,0.25f*s};}
    else if(r00>r11&&r00>r22){const float s=std::sqrt(1+r00-r11-r22)*2;orientation={0.25f*s,(r01+r10)/s,(r02+r20)/s,(r21-r12)/s};}
    else if(r11>r22){const float s=std::sqrt(1+r11-r00-r22)*2;orientation={(r01+r10)/s,0.25f*s,(r12+r21)/s,(r02-r20)/s};}
    else{const float s=std::sqrt(1+r22-r00-r11)*2;orientation={(r02+r20)/s,(r12+r21)/s,0.25f*s,(r10-r01)/s};}
    orientation=normalize(orientation);
}
inline Mat4 perspective(float fovY, float aspect, float nearZ, float farZ) {
    Mat4 m{}; const float f=1.0f/std::tan(fovY*0.5f);
    m.v[0]=f/aspect; m.v[5]=f; m.v[10]=(farZ+nearZ)/(nearZ-farZ); m.v[11]=-1.0f;
    m.v[14]=(2.0f*farZ*nearZ)/(nearZ-farZ); return m;
}
inline Mat4 orthographic(float left,float right,float bottom,float top,float nearZ,float farZ){Mat4 m=Mat4::identity();m.v[0]=2.0f/(right-left);m.v[5]=2.0f/(top-bottom);m.v[10]=-2.0f/(farZ-nearZ);m.v[12]=-(right+left)/(right-left);m.v[13]=-(top+bottom)/(top-bottom);m.v[14]=-(farZ+nearZ)/(farZ-nearZ);return m;}
inline Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 f=normalize(center-eye), s=normalize(cross(f,up)), u=cross(s,f);
    Mat4 m=Mat4::identity();
    m.v[0]=s.x; m.v[1]=u.x; m.v[2]=-f.x;
    m.v[4]=s.y; m.v[5]=u.y; m.v[6]=-f.y;
    m.v[8]=s.z; m.v[9]=u.z; m.v[10]=-f.z;
    m.v[12]=-dot(s,eye); m.v[13]=-dot(u,eye); m.v[14]=dot(f,eye); return m;
}

// Use this when the caller already owns an authoritative view direction.
// Constructing center as eye + direction and subtracting eye again loses
// precision as a player travels away from the world origin.  Viewmodel aiming
// is sensitive to even small basis errors, so avoid that cancellation entirely.
inline Mat4 lookAtDirection(Vec3 eye, Vec3 direction, Vec3 up) {
    const Vec3 f=normalize(direction), s=normalize(cross(f,up)), u=cross(s,f);
    Mat4 m=Mat4::identity();
    m.v[0]=s.x; m.v[1]=u.x; m.v[2]=-f.x;
    m.v[4]=s.y; m.v[5]=u.y; m.v[6]=-f.y;
    m.v[8]=s.z; m.v[9]=u.z; m.v[10]=-f.z;
    m.v[12]=-dot(s,eye); m.v[13]=-dot(u,eye); m.v[14]=dot(f,eye); return m;
}

struct FrustumPlane {
    Vec3 normal{};
    float distance{};
};

struct Frustum {
    std::array<FrustumPlane, 6> planes{};
};

inline Frustum extractFrustum(const Mat4& m) {
    Frustum f;
    const auto makePlane = [](float a, float b, float c, float d) -> FrustumPlane {
        const float len = std::sqrt(a * a + b * b + c * c);
        if (len > 1e-8f) {
            const float inv = 1.0f / len;
            return {Vec3{a * inv, b * inv, c * inv}, d * inv};
        }
        return {Vec3{0, 0, 1}, 0.0f};
    };

    // Left
    f.planes[0] = makePlane(m.v[3] + m.v[0], m.v[7] + m.v[4], m.v[11] + m.v[8], m.v[15] + m.v[12]);
    // Right
    f.planes[1] = makePlane(m.v[3] - m.v[0], m.v[7] - m.v[4], m.v[11] - m.v[8], m.v[15] - m.v[12]);
    // Bottom
    f.planes[2] = makePlane(m.v[3] + m.v[1], m.v[7] + m.v[5], m.v[11] + m.v[9], m.v[15] + m.v[13]);
    // Top
    f.planes[3] = makePlane(m.v[3] - m.v[1], m.v[7] - m.v[5], m.v[11] - m.v[9], m.v[15] - m.v[13]);
    // Near
    f.planes[4] = makePlane(m.v[3] + m.v[2], m.v[7] + m.v[6], m.v[11] + m.v[10], m.v[15] + m.v[14]);
    // Far
    f.planes[5] = makePlane(m.v[3] - m.v[2], m.v[7] - m.v[6], m.v[11] - m.v[10], m.v[15] - m.v[14]);

    return f;
}

inline bool isAabbInFrustum(const Frustum& f, const Vec3& minimum, const Vec3& maximum) {
    for (int i = 0; i < 6; ++i) {
        const auto& p = f.planes[i];
        const float px = p.normal.x >= 0.0f ? maximum.x : minimum.x;
        const float py = p.normal.y >= 0.0f ? maximum.y : minimum.y;
        const float pz = p.normal.z >= 0.0f ? maximum.z : minimum.z;
        if (p.normal.x * px + p.normal.y * py + p.normal.z * pz + p.distance < 0.0f) {
            return false;
        }
    }
    return true;
}

} // namespace scene
