#pragma once
#include "scene/BoundedJson.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numeric>
#include <future>
#include <thread>
#include <atomic>

namespace render::environment {
// A native dome is not a generic spherical projection. Its triangles preserve
// the exported, non-linear UV layout. A BVH bounds one-time panorama conversion.
class NativeSkyDome {
    using V=std::array<double,3>;
    struct Vertex { V p; std::array<double,2> uv; };
    struct Triangle { std::array<std::size_t,3> v; V lo,hi,center; };
    struct Node { V lo,hi; std::size_t begin{},end{},left{},right{}; };
    std::vector<Vertex> vertices_;
    std::vector<Triangle> triangles_;
    std::vector<Node> nodes_;
    static V sub(V a,V b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
    static V cross(V a,V b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
    static double dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
    std::size_t build(std::size_t begin,std::size_t end){
        Node n; n.begin=begin;n.end=end;n.lo.fill(std::numeric_limits<double>::infinity());n.hi.fill(-std::numeric_limits<double>::infinity());
        for(auto i=begin;i<end;++i)for(int a=0;a<3;++a){n.lo[a]=std::min(n.lo[a],triangles_[i].lo[a]);n.hi[a]=std::max(n.hi[a],triangles_[i].hi[a]);}
        const auto id=nodes_.size();nodes_.push_back(n);
        if(end-begin>8){int axis=0;for(int a=1;a<3;++a)if(n.hi[a]-n.lo[a]>n.hi[axis]-n.lo[axis])axis=a;
            const auto mid=(begin+end)/2;std::nth_element(triangles_.begin()+begin,triangles_.begin()+mid,triangles_.begin()+end,[axis](const auto& a,const auto& b){return a.center[axis]<b.center[axis];});
            const auto left=build(begin,mid),right=build(mid,end);nodes_[id].left=left;nodes_[id].right=right;}
        return id;
    }
    static bool intersects(const Node& n,V d,double nearest){
        double lo=0,hi=nearest;
        for(int a=0;a<3;++a){if(std::abs(d[a])<1e-14){if(n.lo[a]>0||n.hi[a]<0)return false;continue;}
            double x=n.lo[a]/d[a],y=n.hi[a]/d[a];if(x>y)std::swap(x,y);lo=std::max(lo,x);hi=std::min(hi,y);if(lo>hi)return false;}
        return true;
    }
    void query(std::size_t id,V direction,double& nearest,std::array<double,2>& uv,bool& hit)const{
        const auto& n=nodes_[id];if(!intersects(n,direction,nearest))return;
        if(n.left){query(n.left,direction,nearest,uv,hit);query(n.right,direction,nearest,uv,hit);return;}
        for(auto i=n.begin;i<n.end;++i){const auto& t=triangles_[i];const auto& a=vertices_[t.v[0]];const auto& b=vertices_[t.v[1]];const auto& c=vertices_[t.v[2]];
            const auto e1=sub(b.p,a.p),e2=sub(c.p,a.p),p=cross(direction,e2);const auto det=dot(e1,p);if(std::abs(det)<1e-12)continue;
            const V origin{-a.p[0],-a.p[1],-a.p[2]};const auto u=dot(origin,p)/det;if(u< -1e-8||u>1+1e-8)continue;
            const auto q=cross(origin,e1);const auto v=dot(direction,q)/det;if(v< -1e-8||u+v>1+1e-8)continue;
            const auto distance=dot(e2,q)/det;if(distance<=0||distance>=nearest)continue;
            nearest=distance;hit=true;for(int axis=0;axis<2;++axis)uv[axis]=a.uv[axis]*(1-u-v)+b.uv[axis]*u+c.uv[axis]*v;
        }
    }
public:
    bool load(const std::filesystem::path& path,std::string& error){
        error.clear();vertices_.clear();triangles_.clear();nodes_.clear();
        try{
            if(std::filesystem::file_size(path)>4*1024*1024)throw std::runtime_error("Native sky mapping exceeds 4 MiB");
            std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Native sky mapping unavailable");
            const std::string text((std::istreambuf_iterator<char>(file)),{});const auto json=scene::codm::parseJson(text);
            const auto& positions=json.at("positions");const auto& uv=json.at("uv");const auto& indices=json.at("indices");
            if(!positions.is_array()||positions.empty()||positions.size()>65536||uv.size()!=positions.size()||!indices.is_array()||indices.empty()||indices.size()%3||indices.size()>196608)throw std::runtime_error("Invalid native sky mesh counts");
            for(std::size_t i=0;i<positions.size();++i){Vertex v{positions[i].get<V>(),uv[i].get<std::array<double,2>>()};
                // NIF root-to-mesh transforms applied in reverse to local points.
                if(json.contains("nodesRootToMesh"))for(auto node=json["nodesRootToMesh"].rbegin();node!=json["nodesRootToMesh"].rend();++node){const auto r=node->at("rotation").get<std::array<double,9>>();const auto t=node->at("translation").get<V>();const double s=node->at("scale").get<double>();V p{};for(int a=0;a<3;++a)p[a]=t[a]+s*(r[a*3]*v.p[0]+r[a*3+1]*v.p[1]+r[a*3+2]*v.p[2]);v.p=p;}
                for(double x:v.p)if(!std::isfinite(x))throw std::runtime_error("Non-finite native sky vertex");for(double x:v.uv)if(!std::isfinite(x))throw std::runtime_error("Non-finite native sky UV");vertices_.push_back(v);}
            for(std::size_t i=0;i<indices.size();i+=3){Triangle t;for(int c=0;c<3;++c){t.v[c]=indices[i+c].get<std::size_t>();if(t.v[c]>=vertices_.size())throw std::runtime_error("Native sky index out of range");}
                for(int a=0;a<3;++a){t.lo[a]=std::min({vertices_[t.v[0]].p[a],vertices_[t.v[1]].p[a],vertices_[t.v[2]].p[a]});t.hi[a]=std::max({vertices_[t.v[0]].p[a],vertices_[t.v[1]].p[a],vertices_[t.v[2]].p[a]});t.center[a]=(t.lo[a]+t.hi[a])*.5;}triangles_.push_back(t);}
            build(0,triangles_.size());return true;
        }catch(const std::exception& e){vertices_.clear();triangles_.clear();nodes_.clear();error=e.what();return false;}
    }
    bool sample(V direction,std::array<double,2>& uv)const{
        for(double x:direction)if(!std::isfinite(x))return false;
        if(nodes_.empty()||dot(direction,direction)<1e-20)return false;
        double nearest=std::numeric_limits<double>::infinity();bool hit=false;query(0,direction,nearest,uv,hit);return hit;
    }
    bool panorama(const std::vector<std::uint8_t>& rgba,unsigned width,unsigned height,unsigned outputWidth,unsigned outputHeight,std::vector<std::uint8_t>& output,std::string& error,bool parallel=true)const{
        if(!width||!height||width>16384||height>16384||rgba.size()!=static_cast<std::size_t>(width)*height*4||!outputWidth||!outputHeight||outputWidth>8192||outputHeight>4096||nodes_.empty()){error="Invalid native sky conversion dimensions";return false;}
        output.assign(static_cast<std::size_t>(outputWidth)*outputHeight*4,255);
        constexpr double pi=3.14159265358979323846;
        const auto colorAt=[&](const std::array<double,2>& uv){
            const double sx=std::clamp(uv[0],0.,1.)*width-.5,sy=std::clamp(uv[1],0.,1.)*height-.5;const int ix=static_cast<int>(std::floor(sx)),iy=static_cast<int>(std::floor(sy));const double fx=sx-ix,fy=sy-iy;
            std::array<double,3> color{};for(int c=0;c<3;++c)for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto source=(static_cast<std::size_t>(std::clamp(iy+dy,0,static_cast<int>(height)-1))*width+std::clamp(ix+dx,0,static_cast<int>(width)-1))*4;color[c]+=rgba[source+c]*(dx?fx:1-fx)*(dy?fy:1-fy);}return color;
        };
        // Only the unmodelled lower domain is synthesized. Find the actual rim
        // per azimuth (slightly below horizontal in the authored mesh), retaining
        // continuity there, then converge to its mean instead of a pinched star.
        struct Rim { double latitude{};std::array<double,3> color{}; };
        std::vector<Rim> rim(outputWidth);std::array<double,3> mean{};
        for(unsigned x=0;x<outputWidth;++x){const double longitude=((x+.5)/outputWidth-.5)*2*pi;const double cx=std::cos(longitude),cy=std::sin(longitude);std::array<double,2> uv{};
            if(!sample({cx,cy,0},uv)){error="Native sky mapping has an uncovered horizon";output.clear();return false;}
            double found=0,missing=-pi/2;for(int iteration=0;iteration<24;++iteration){const double mid=(found+missing)*.5;std::array<double,2> candidate{};if(sample({std::cos(mid)*cx,std::cos(mid)*cy,std::sin(mid)},candidate)){found=mid;uv=candidate;}else missing=mid;}
            rim[x]={found,colorAt(uv)};for(int c=0;c<3;++c)mean[c]+=rim[x].color[c]/outputWidth;
        }
        std::atomic<bool> valid{true};
        const auto rows=[&](unsigned first,unsigned last){
        for(unsigned y=first;y<last;++y){const double latitude=((y+.5)/outputHeight-.5)*pi;for(unsigned x=0;x<outputWidth;++x){const double longitude=((x+.5)/outputWidth-.5)*2*pi;V d{std::cos(latitude)*std::cos(longitude),std::cos(latitude)*std::sin(longitude),std::sin(latitude)};std::array<double,2> uv{};bool hit=sample(d,uv);
            std::array<double,3> color{};
            if(hit)color=colorAt(uv);
            else if(latitude<rim[x].latitude){const double t=std::clamp((rim[x].latitude-latitude)/(pi/18),0.,1.);const double blend=t*t*(3-2*t);for(int c=0;c<3;++c)color[c]=rim[x].color[c]*(1-blend)+mean[c]*blend;}
            else {valid=false;return;}
            const auto target=(static_cast<std::size_t>(y)*outputWidth+x)*4;for(int c=0;c<3;++c)output[target+c]=static_cast<std::uint8_t>(std::clamp(std::lround(color[c]),0l,255l));
        }}};
        const unsigned workers=parallel?std::min({8u,std::max(1u,std::thread::hardware_concurrency()),outputHeight}):1u;
        std::vector<std::future<void>> jobs;
        for(unsigned i=1;i<workers;++i)jobs.push_back(std::async(std::launch::async,rows,outputHeight*i/workers,outputHeight*(i+1)/workers));
        rows(0,outputHeight/workers);for(auto& job:jobs)job.get();
        if(!valid){error="Native sky mapping has an uncovered upper region";output.clear();return false;}return true;
    }
};
}
