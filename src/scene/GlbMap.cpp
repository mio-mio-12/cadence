#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "scene/GlbMap.h"
#include "scene/SpawnSelection.h"
#include "scene/CodmMaterialMetadata.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace scene::glb {
namespace {

constexpr float kMetersToWorld=100.0f;

Vec3 convertPoint(const float* value,float scaleMultiplier){const float s=kMetersToWorld*scaleMultiplier;return {value[0]*s,-value[2]*s,value[1]*s};}
Vec3 convertVector(const float* value){return normalize(Vec3{value[0],-value[2],value[1]});}

Vec3 transformVector(const Mat4& m,Vec3 p){const auto inv=inverseAffine(m);return normalize(Vec3{inv.v[0]*p.x+inv.v[1]*p.y+inv.v[2]*p.z,inv.v[4]*p.x+inv.v[5]*p.y+inv.v[6]*p.z,inv.v[8]*p.x+inv.v[9]*p.y+inv.v[10]*p.z});}

Mat4 nodeMatrix(const cgltf_node* node,float scaleMultiplier){float source[16];cgltf_node_transform_world(node,source);Mat4 converted=Mat4::identity();
    // C * M * C^-1, with C mapping glTF (X,Y,Z) to app (X,-Z,Y).
    Mat4 m;std::copy(source,source+16,m.v.begin());Mat4 c=Mat4::identity();c.v={1,0,0,0, 0,0,1,0, 0,-1,0,0, 0,0,0,1};
    converted=c*m*inverseAffine(c);const float s=kMetersToWorld*scaleMultiplier;converted.v[12]*=s;converted.v[13]*=s;converted.v[14]*=s;return converted;
}

void growBounds(Bounds& bounds,Vec3 p){if(!bounds.valid){bounds.minimum=bounds.maximum=p;bounds.valid=true;return;}bounds.minimum={std::min(bounds.minimum.x,p.x),std::min(bounds.minimum.y,p.y),std::min(bounds.minimum.z,p.z)};bounds.maximum={std::max(bounds.maximum.x,p.x),std::max(bounds.maximum.y,p.y),std::max(bounds.maximum.z,p.z)};}

std::filesystem::path materialTexture(const cgltf_texture_view* view,const std::filesystem::path& source,const std::filesystem::path& cache,std::unordered_map<const cgltf_image*,std::filesystem::path>& extracted){
    if(!view||!view->texture)return {};const auto* texture=view->texture;const auto* image=texture->has_webp&&texture->webp_image?texture->webp_image:texture->image;if(!image)return {};
    if(image->uri&&image->uri[0]&&std::string_view(image->uri).find("data:")!=0)return source.parent_path()/std::filesystem::u8path(image->uri);
    if(const auto found=extracted.find(image);found!=extracted.end())return found->second;if(!image->buffer_view||!image->buffer_view->buffer||!image->buffer_view->buffer->data)return {};
    std::string extension=".bin";const std::string mime=image->mime_type?image->mime_type:"";if(mime.find("png")!=std::string::npos)extension=".png";else if(mime.find("jpeg")!=std::string::npos||mime.find("jpg")!=std::string::npos)extension=".jpg";else if(mime.find("webp")!=std::string::npos)extension=".webp";
    std::error_code ec;std::filesystem::create_directories(cache,ec);const auto path=cache/("image_"+std::to_string(extracted.size())+extension);const auto* bytes=static_cast<const std::uint8_t*>(image->buffer_view->buffer->data)+image->buffer_view->offset;std::ofstream output(path,std::ios::binary|std::ios::trunc);if(!output)return {};output.write(reinterpret_cast<const char*>(bytes),static_cast<std::streamsize>(image->buffer_view->size));if(!output)return {};extracted.emplace(image,path);return path;
}

float edge(Vec2 a,Vec2 b,Vec2 p){return (p.x-a.x)*(b.y-a.y)-(p.y-a.y)*(b.x-a.x);}
bool xyInside(const CollisionTriangle& t,float x,float y){
    const Vec2 p{x,y},a{t.a.x,t.a.y},b{t.b.x,t.b.y},c{t.c.x,t.c.y};
    const float e0=edge(a,b,p),e1=edge(b,c,p),e2=edge(c,a,p);
    constexpr float eps = 0.5f;
    return (e0>=-eps&&e1>=-eps&&e2>=-eps)||(e0<=eps&&e1<=eps&&e2<=eps);
}

Vec3 closestPoint(Vec3 p,Vec3 a,Vec3 b,Vec3 c){const Vec3 ab=b-a,ac=c-a,ap=p-a;const float d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0&&d2<=0)return a;const Vec3 bp=p-b;const float d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return b;const float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+ab*(d1/(d1-d3));const Vec3 cp=p-c;const float d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return c;const float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+ac*(d2/(d2-d6));const float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return b+(c-b)*((d4-d3)/((d4-d3)+(d5-d6)));const float denom=1.0f/(va+vb+vc),v=vb*denom,w=vc*denom;return a+ab*v+ac*w;}

std::string canonical(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;}

bool isDecalSurface(std::string_view name){
    constexpr std::array<std::string_view,23> tokens={"decal","dec_","graffiti","graphitti","poster","paper","calender","bulletin","bullet_hole","bulletmark","stain","splatter","blood_splat","light_hotspot","lensflare","godray","switch","socket","patch","tarp","grate","drain","debris"};
    return std::any_of(tokens.begin(),tokens.end(),[&](std::string_view token){return name.find(token)!=std::string_view::npos;});
}

bool isNonSolidName(std::string_view name){
    constexpr std::array<std::string_view,10> tokens={"noclip","nosolid","non_solid","nonsolid","no_collision","nocollision","ghost","trigger","nodraw","passable"};
    return std::any_of(tokens.begin(),tokens.end(),[&](std::string_view token){return name.find(token)!=std::string_view::npos;});
}

constexpr auto noRange = std::numeric_limits<std::uint32_t>::max();

void buildRanges(CollisionRangeIndex& index,
                 const std::vector<std::vector<std::uint32_t>>& cells,
                 const std::vector<std::uint32_t>& global,
                 const std::vector<CollisionTriangle>& triangles){
    index.nodes.clear();
    index.roots.assign(cells.size()+1, noRange);
    const auto build=[&](auto&& self, const auto& list, std::uint32_t begin, std::uint32_t end)->std::uint32_t{
        const auto slot=static_cast<std::uint32_t>(index.nodes.size());
        index.nodes.emplace_back();
        CollisionRangeIndex::Node node;
        node.begin=begin;node.end=end;node.leaf=end-begin<=16;
        if(node.leaf){
            const float inf=std::numeric_limits<float>::infinity();
            node.minimum={inf,inf,inf};node.maximum={-inf,-inf,-inf};node.groundMinimum=inf;
            for(auto i=begin;i<end;++i){
                const auto& t=triangles[list[i]];
                // An unusual non-finite bound must disable pruning, not hide
                // geometry that the original narrow phase would have tested.
                if(!std::isfinite(t.minimum.x)||!std::isfinite(t.minimum.y)||!std::isfinite(t.minimum.z)||
                   !std::isfinite(t.maximum.x)||!std::isfinite(t.maximum.y)||!std::isfinite(t.maximum.z)){
                    node.minimum={-inf,-inf,-inf};node.maximum={inf,inf,inf};node.groundMinimum=-inf;break;
                }
                node.minimum={std::min(node.minimum.x,t.minimum.x),std::min(node.minimum.y,t.minimum.y),std::min(node.minimum.z,t.minimum.z)};
                node.maximum={std::max(node.maximum.x,t.maximum.x),std::max(node.maximum.y,t.maximum.y),std::max(node.maximum.z,t.maximum.z)};
                // groundHeight accepts a padded XY box and tolerant triangle
                // edges. Bound the plane over that WHOLE box, not vertex Z:
                // otherwise thin/sloped triangles could change support.
                if(t.walkable){
                    if(!std::isfinite(t.normal.z)||std::abs(t.normal.z)<1e-5f)node.groundMinimum=-inf;
                    else for(float x:{t.minimum.x-1.f,t.maximum.x+1.f})for(float y:{t.minimum.y-1.f,t.maximum.y+1.f}){
                        const double dx=double(x)-t.a.x,dy=double(y)-t.a.y;
                        const double sx=dx*t.normal.x/t.normal.z,sy=dy*t.normal.y/t.normal.z;
                        const double z=double(t.a.z)-sx-sy;
                        const double margin=.01+32*std::numeric_limits<float>::epsilon()*(std::abs(t.a.z)+std::abs(sx)+std::abs(sy));
                        node.groundMinimum=std::min(node.groundMinimum,std::isfinite(z)?float(z-margin):-inf);
                    }
                }
            }
        }else{
            const auto middle=begin+(end-begin)/2;
            const auto left=self(self,list,begin,middle),right=self(self,list,middle,end);
            const auto& a=index.nodes[left];const auto& b=index.nodes[right];
            node.minimum={std::min(a.minimum.x,b.minimum.x),std::min(a.minimum.y,b.minimum.y),std::min(a.minimum.z,b.minimum.z)};
            node.maximum={std::max(a.maximum.x,b.maximum.x),std::max(a.maximum.y,b.maximum.y),std::max(a.maximum.z,b.maximum.z)};
            node.groundMinimum=std::min(a.groundMinimum,b.groundMinimum);
        }
        node.escape=static_cast<std::uint32_t>(index.nodes.size());
        index.nodes[slot]=node;
        return slot;
    };
    for(std::size_t cell=0;cell<=cells.size();++cell){
        const auto& list=cell<cells.size()?cells[cell]:global;
        if(list.size()>32) index.roots[cell]=build(build,list,0,static_cast<std::uint32_t>(list.size()));
    }
}

// Preorder traversal uses escape offsets instead of a temporary stack. It
// visits surviving indices in precisely the same order as the linear list.
// Moving queries must recompute overlap from their current position and stamp
// skipped ranges when a triangle can also occur in another visited cell.
template<class Overlaps,class Test,class Skip>
bool visitRanges(const CollisionRangeIndex& index,std::size_t cell,
                 const std::vector<std::uint32_t>& list,Overlaps&& overlaps,Test&& test,Skip&& skip){
    const auto root=cell<index.roots.size()?index.roots[cell]:noRange;
    if(root==noRange){
        for(const auto triangle:list) if(!test(triangle))return false;
        return true;
    }
    const auto end=index.nodes[root].escape;
    for(auto i=root;i<end;){
        const auto& node=index.nodes[i];
        if(!overlaps(node)){skip(node.begin,node.end);i=node.escape;continue;}
        if(node.leaf) for(auto j=node.begin;j<node.end;++j) if(!test(list[j]))return false;
        ++i;
    }
    return true;
}
template<class Overlaps,class Test>
bool visitRanges(const CollisionRangeIndex& index,std::size_t cell,
                 const std::vector<std::uint32_t>& list,Overlaps&& overlaps,Test&& test){
    return visitRanges(index,cell,list,overlaps,test,[](auto,auto){});
}

} // namespace

void Map::buildCollisionIndex(){
    shotGeometry.reset();
    if(authoredCollision.present && !scene.meshes.empty()){
        auto geometry=std::make_shared<Map>();
        for(const auto& mesh:scene.meshes){
            if(mesh.weatherNonBlocking || mesh.color.w<=.001f || mesh.forceAlpha || mesh.decal || mesh.decalAdditive || mesh.decalMultiply || mesh.lens)continue;
            for(std::size_t i=0;i+2<mesh.indices.size();i+=3){
                const auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];
                if(a>=mesh.vertices.size()||b>=mesh.vertices.size()||c>=mesh.vertices.size())continue;
                if(mesh.useVertexColor && std::max({mesh.vertices[a].color.w,mesh.vertices[b].color.w,mesh.vertices[c].color.w})<=.001f)continue;
                CollisionTriangle t;
                t.a=transformPoint(mesh.modelTransform,mesh.vertices[a].position);
                t.b=transformPoint(mesh.modelTransform,mesh.vertices[b].position);
                t.c=transformPoint(mesh.modelTransform,mesh.vertices[c].position);
                const auto n=cross(t.b-t.a,t.c-t.a);if(length(n)<1e-7f)continue;t.normal=normalize(n);
                t.minimum={std::min({t.a.x,t.b.x,t.c.x}),std::min({t.a.y,t.b.y,t.c.y}),std::min({t.a.z,t.b.z,t.c.z})};
                t.maximum={std::max({t.a.x,t.b.x,t.c.x}),std::max({t.a.y,t.b.y,t.c.y}),std::max({t.a.z,t.b.z,t.c.z})};
                t.blocking=true;geometry->collision.push_back(t);
            }
        }
        geometry->buildCollisionIndex();shotGeometry=std::move(geometry);
    }
    static std::atomic<std::uint64_t> nextRevision{1};
    collisionRevision=nextRevision.fetch_add(1,std::memory_order_relaxed);
    globalCollision.clear();
    globalWalkable.clear();
    globalBlocking.clear();
    triangleTag.assign(collision.size(), 0);
    queryEpoch = 1;
    walkableRanges.nodes.clear();walkableRanges.roots.clear();
    blockingRanges.nodes.clear();blockingRanges.roots.clear();

    if(collision.empty()){
        gridWidth = gridHeight = 0;
        gridCollision.clear();
        gridWalkable.clear();
        gridBlocking.clear();
        return;
    }

    boundsMinX = std::numeric_limits<float>::max();
    boundsMinY = std::numeric_limits<float>::max();
    boundsMaxX = -std::numeric_limits<float>::max();
    boundsMaxY = -std::numeric_limits<float>::max();

    for(const auto& tri : collision){
        boundsMinX = std::min({boundsMinX, tri.a.x, tri.b.x, tri.c.x});
        boundsMinY = std::min({boundsMinY, tri.a.y, tri.b.y, tri.c.y});
        boundsMaxX = std::max({boundsMaxX, tri.a.x, tri.b.x, tri.c.x});
        boundsMaxY = std::max({boundsMaxY, tri.a.y, tri.b.y, tri.c.y});
    }

    boundsMinX -= collisionCellSize * 2.0f;
    boundsMinY -= collisionCellSize * 2.0f;
    boundsMaxX += collisionCellSize * 2.0f;
    boundsMaxY += collisionCellSize * 2.0f;

    gridWidth = std::max(1, static_cast<int>(std::ceil((boundsMaxX - boundsMinX) / collisionCellSize)));
    gridHeight = std::max(1, static_cast<int>(std::ceil((boundsMaxY - boundsMinY) / collisionCellSize)));

    const std::size_t totalCells = static_cast<std::size_t>(gridWidth) * static_cast<std::size_t>(gridHeight);
    gridCollision.assign(totalCells, {});
    gridWalkable.assign(totalCells, {});
    gridBlocking.assign(totalCells, {});

    for(std::uint32_t index=0;index<collision.size();++index){
        const auto& triangle=collision[index];
        const int minX=gridCoordX(triangle.minimum.x),maxX=gridCoordX(triangle.maximum.x);
        const int minY=gridCoordY(triangle.minimum.y),maxY=gridCoordY(triangle.maximum.y);
        const std::uint64_t cells=static_cast<std::uint64_t>(std::max(1, maxX-minX+1))*static_cast<std::uint64_t>(std::max(1, maxY-minY+1));
        if(cells>1024 || minX < 0 || maxX >= gridWidth || minY < 0 || maxY >= gridHeight){
            globalCollision.push_back(index);
            if(triangle.walkable) globalWalkable.push_back(index);
            if(triangle.blocking) globalBlocking.push_back(index);
            continue;
        }
        for(int y=minY;y<=maxY;++y){
            for(int x=minX;x<=maxX;++x){
                const int cell = cellIndex(x, y);
                if(cell >= 0 && cell < static_cast<int>(totalCells)){
                    gridCollision[cell].push_back(index);
                    if(triangle.walkable) gridWalkable[cell].push_back(index);
                    if(triangle.blocking) gridBlocking[cell].push_back(index);
                }
            }
        }
    }
    buildRanges(walkableRanges,gridWalkable,globalWalkable,collision);
    buildRanges(blockingRanges,gridBlocking,globalBlocking,collision);
}

float Map::groundHeight(float x,float y,float referenceZ,float fallback) const{
    float result=fallback;
    const auto test=[&](std::uint32_t index){
        const auto& triangle=collision[index];
        if(!triangle.walkable||x<triangle.minimum.x-1.0f||x>triangle.maximum.x+1.0f||y<triangle.minimum.y-1.0f||y>triangle.maximum.y+1.0f||std::abs(triangle.normal.z)<1e-5f||!xyInside(triangle,x,y))return;
        const float z=triangle.a.z-(triangle.normal.x*(x-triangle.a.x)+triangle.normal.y*(y-triangle.a.y))/triangle.normal.z;
        if(z<=referenceZ+45.0f&&z>result)result=z;
    };
    const auto overlaps=[&](const auto& node){
        return !(x<node.minimum.x-1.0f||x>node.maximum.x+1.0f||y<node.minimum.y-1.0f||y>node.maximum.y+1.0f||node.groundMinimum>referenceZ+45.f);
    };
    const auto visit=[&](std::uint32_t index){test(index);return true;};
    const int idx = cellIndex(gridCoordX(x), gridCoordY(y));
    if(idx >= 0 && idx < static_cast<int>(gridWalkable.size())){
        visitRanges(walkableRanges,idx,gridWalkable[idx],overlaps,visit);
    }
    visitRanges(walkableRanges,gridWalkable.size(),globalWalkable,overlaps,visit);
    return result;
}

float Map::navigationGroundHeight(float x,float y,float referenceZ,float fallback,float footprintRadius) const{
    constexpr float noGround=-std::numeric_limits<float>::max()*0.25f;
    float best=groundHeight(x,y,referenceZ,noGround);
    if(footprintRadius > 1.0f){
        const float r = footprintRadius * 0.75f;
        const std::array<Vec2,8> offsets={
            Vec2{r, 0}, Vec2{-r, 0}, Vec2{0, r}, Vec2{0, -r},
            Vec2{r*0.707f, r*0.707f}, Vec2{-r*0.707f, r*0.707f},
            Vec2{r*0.707f, -r*0.707f}, Vec2{-r*0.707f, -r*0.707f}
        };
        for(const auto& offset:offsets){
            const float sample=groundHeight(x+offset.x,y+offset.y,referenceZ,noGround);
            if(sample>noGround*0.5f && (best<=noGround*0.5f || sample>best)) best = sample;
        }
    }
    return best > noGround * 0.5f ? best : fallback;
}

std::optional<Vec3> Map::raycastWalkable(Vec3 origin,Vec3 direction,float maxDistance) const{
    direction=normalize(direction);float nearest=maxDistance;std::optional<Vec3> hit;
    const Vec3 to = origin + direction * maxDistance;
    const auto overlaps=[&](const auto& node){return !(std::max(origin.x,to.x)+1<node.minimum.x||std::min(origin.x,to.x)-1>node.maximum.x||std::max(origin.y,to.y)+1<node.minimum.y||std::min(origin.y,to.y)-1>node.maximum.y||std::max(origin.z,to.z)+1<node.minimum.z||std::min(origin.z,to.z)-1>node.maximum.z);};

    uint32_t epoch = ++queryEpoch;
    if(epoch == 0){
        std::fill(triangleTag.begin(), triangleTag.end(), 0);
        epoch = queryEpoch = 1;
    }

    const auto testTriangle = [&](std::uint32_t index){
        if(triangleTag[index] == epoch) return;
        triangleTag[index] = epoch;
        const auto& triangle=collision[index];
        if(!triangle.walkable)return;
        const Vec3 edge1=triangle.b-triangle.a,edge2=triangle.c-triangle.a,p=cross(direction,edge2);
        const float determinant=dot(edge1,p);
        if(std::abs(determinant)<1e-6f)return;
        const float inverse=1.0f/determinant;
        const Vec3 offset=origin-triangle.a;
        const float u=dot(offset,p)*inverse;
        if(u<0.0f||u>1.0f)return;
        const Vec3 q=cross(offset,edge1);
        const float v=dot(direction,q)*inverse;
        if(v<0.0f||u+v>1.0f)return;
        const float distance=dot(edge2,q)*inverse;
        if(distance>0.0f&&distance<nearest){nearest=distance;hit=origin+direction*distance;}
    };

    int curX = gridCoordX(origin.x), curY = gridCoordY(origin.y);
    const int endX = gridCoordX(to.x), endY = gridCoordY(to.y);
    const int stepX = (to.x > origin.x) ? 1 : ((to.x < origin.x) ? -1 : 0);
    const int stepY = (to.y > origin.y) ? 1 : ((to.y < origin.y) ? -1 : 0);
    const float dx = std::abs(to.x - origin.x), dy = std::abs(to.y - origin.y);
    const float tDeltaX = (dx > 1e-4f) ? (collisionCellSize / dx) : std::numeric_limits<float>::max();
    const float tDeltaY = (dy > 1e-4f) ? (collisionCellSize / dy) : std::numeric_limits<float>::max();
    const float cellMinX = boundsMinX + curX * collisionCellSize, cellMinY = boundsMinY + curY * collisionCellSize;
    float tMaxX = (stepX > 0) ? ((cellMinX + collisionCellSize - origin.x) / std::max(dx, 1e-4f)) : ((origin.x - cellMinX) / std::max(dx, 1e-4f));
    float tMaxY = (stepY > 0) ? ((cellMinY + collisionCellSize - origin.y) / std::max(dy, 1e-4f)) : ((origin.y - cellMinY) / std::max(dy, 1e-4f));

    while(true){
        const int idx = cellIndex(curX, curY);
        if(idx >= 0 && idx < static_cast<int>(gridWalkable.size())){
            visitRanges(walkableRanges,idx,gridWalkable[idx],overlaps,[&](auto index){testTriangle(index);return true;});
        }
        if(curX == endX && curY == endY) break;
        if(tMaxX < tMaxY){ tMaxX += tDeltaX; curX += stepX; }
        else { tMaxY += tDeltaY; curY += stepY; }
    }
    visitRanges(walkableRanges,gridWalkable.size(),globalWalkable,overlaps,[&](auto index){testTriangle(index);return true;});
    return hit;
}

std::optional<Map::RaycastHit> Map::raycastSurface(Vec3 origin,Vec3 direction,float maxDistance) const{
    direction=normalize(direction);float nearest=maxDistance;std::optional<RaycastHit> hit;
    const Vec3 to = origin + direction * maxDistance;
    const auto overlaps=[&](const auto& node){return !(std::max(origin.x,to.x)+1<node.minimum.x||std::min(origin.x,to.x)-1>node.maximum.x||std::max(origin.y,to.y)+1<node.minimum.y||std::min(origin.y,to.y)-1>node.maximum.y||std::max(origin.z,to.z)+1<node.minimum.z||std::min(origin.z,to.z)-1>node.maximum.z);};

    uint32_t epoch = ++queryEpoch;
    if(epoch == 0){
        std::fill(triangleTag.begin(), triangleTag.end(), 0);
        epoch = queryEpoch = 1;
    }

    const auto testTriangle = [&](std::uint32_t index){
        if(triangleTag[index] == epoch) return;
        triangleTag[index] = epoch;
        const auto& triangle=collision[index];
        const Vec3 edge1=triangle.b-triangle.a,edge2=triangle.c-triangle.a,p=cross(direction,edge2);
        const float determinant=dot(edge1,p);
        if(std::abs(determinant)<1e-6f)return;
        const float inverse=1.0f/determinant;
        const Vec3 offset=origin-triangle.a;
        const float u=dot(offset,p)*inverse;
        if(u<0.0f||u>1.0f)return;
        const Vec3 q=cross(offset,edge1);
        const float v=dot(direction,q)*inverse;
        if(v<0.0f||u+v>1.0f)return;
        const float distance=dot(edge2,q)*inverse;
        if(distance>0.0f&&distance<nearest){
            nearest=distance;
            Vec3 norm=normalize(cross(edge1,edge2));
            if(dot(norm,direction)>0.0f) norm=norm*-1.0f;
            hit=RaycastHit{origin+direction*distance,norm,distance};
        }
    };

    int curX = gridCoordX(origin.x), curY = gridCoordY(origin.y);
    const int endX = gridCoordX(to.x), endY = gridCoordY(to.y);
    const int stepX = (to.x > origin.x) ? 1 : ((to.x < origin.x) ? -1 : 0);
    const int stepY = (to.y > origin.y) ? 1 : ((to.y < origin.y) ? -1 : 0);
    const float dx = std::abs(to.x - origin.x), dy = std::abs(to.y - origin.y);
    const float tDeltaX = (dx > 1e-4f) ? (collisionCellSize / dx) : std::numeric_limits<float>::max();
    const float tDeltaY = (dy > 1e-4f) ? (collisionCellSize / dy) : std::numeric_limits<float>::max();
    const float cellMinX = boundsMinX + curX * collisionCellSize, cellMinY = boundsMinY + curY * collisionCellSize;
    float tMaxX = (stepX > 0) ? ((cellMinX + collisionCellSize - origin.x) / std::max(dx, 1e-4f)) : ((origin.x - cellMinX) / std::max(dx, 1e-4f));
    float tMaxY = (stepY > 0) ? ((cellMinY + collisionCellSize - origin.y) / std::max(dy, 1e-4f)) : ((origin.y - cellMinY) / std::max(dy, 1e-4f));

    while(true){
        const int idx = cellIndex(curX, curY);
        if(idx >= 0 && idx < static_cast<int>(gridBlocking.size())){
            visitRanges(blockingRanges,idx,gridBlocking[idx],overlaps,[&](auto index){testTriangle(index);return true;});
        }
        if(idx >= 0 && idx < static_cast<int>(gridWalkable.size())){
            visitRanges(walkableRanges,idx,gridWalkable[idx],overlaps,[&](auto index){testTriangle(index);return true;});
        }
        if(curX == endX && curY == endY) break;
        if(tMaxX < tMaxY){ tMaxX += tDeltaX; curX += stepX; }
        else { tMaxY += tDeltaY; curY += stepY; }
    }
    visitRanges(blockingRanges,gridBlocking.size(),globalBlocking,overlaps,[&](auto index){testTriangle(index);return true;});
    visitRanges(walkableRanges,gridWalkable.size(),globalWalkable,overlaps,[&](auto index){testTriangle(index);return true;});
    return hit;
}

bool Map::lineOfSight(Vec3 from,Vec3 to) const{
    const Vec3 delta=to-from;
    const float maximum=length(delta);
    if(maximum<1e-3f)return true;
    const Vec3 direction=delta/maximum;

    uint32_t epoch = ++queryEpoch;
    if(epoch == 0){
        std::fill(triangleTag.begin(), triangleTag.end(), 0);
        epoch = queryEpoch = 1;
    }

    const float minRayX = std::min(from.x,to.x) - 1.0f, maxRayX = std::max(from.x,to.x) + 1.0f;
    const float minRayY = std::min(from.y,to.y) - 1.0f, maxRayY = std::max(from.y,to.y) + 1.0f;
    const float minRayZ = std::min(from.z,to.z) - 1.0f, maxRayZ = std::max(from.z,to.z) + 1.0f;
    const auto overlaps=[&](const auto& node){
        return !(maxRayX<node.minimum.x||minRayX>node.maximum.x||maxRayY<node.minimum.y||minRayY>node.maximum.y||maxRayZ<node.minimum.z||minRayZ>node.maximum.z);
    };

    const auto testTriangle = [&](std::uint32_t index) -> bool {
        if(triangleTag[index] == epoch) return true;
        triangleTag[index] = epoch;
        const auto& triangle=collision[index];
        if(!triangle.blocking) return true;
        if(maxRayX<triangle.minimum.x||minRayX>triangle.maximum.x||maxRayY<triangle.minimum.y||minRayY>triangle.maximum.y||maxRayZ<triangle.minimum.z||minRayZ>triangle.maximum.z) return true;
        const Vec3 edge1=triangle.b-triangle.a,edge2=triangle.c-triangle.a,p=cross(direction,edge2);
        const float determinant=dot(edge1,p);
        if(std::abs(determinant)<1e-6f) return true;
        const float inverse=1.0f/determinant;
        const Vec3 offset=from-triangle.a;
        const float u=dot(offset,p)*inverse;
        if(u<0.0f||u>1.0f) return true;
        const Vec3 q=cross(offset,edge1);
        const float v=dot(direction,q)*inverse;
        if(v<0.0f||u+v>1.0f) return true;
        const float distance=dot(edge2,q)*inverse;
        if(distance>0.1f&&distance<maximum-0.1f) return false;
        return true;
    };

    int curX = gridCoordX(from.x), curY = gridCoordY(from.y);
    const int endX = gridCoordX(to.x), endY = gridCoordY(to.y);
    const int stepX = (to.x > from.x) ? 1 : ((to.x < from.x) ? -1 : 0);
    const int stepY = (to.y > from.y) ? 1 : ((to.y < from.y) ? -1 : 0);
    const float dx = std::abs(to.x - from.x), dy = std::abs(to.y - from.y);
    const float tDeltaX = (dx > 1e-4f) ? (collisionCellSize / dx) : std::numeric_limits<float>::max();
    const float tDeltaY = (dy > 1e-4f) ? (collisionCellSize / dy) : std::numeric_limits<float>::max();
    const float cellMinX = boundsMinX + curX * collisionCellSize, cellMinY = boundsMinY + curY * collisionCellSize;
    float tMaxX = (stepX > 0) ? ((cellMinX + collisionCellSize - from.x) / std::max(dx, 1e-4f)) : ((from.x - cellMinX) / std::max(dx, 1e-4f));
    float tMaxY = (stepY > 0) ? ((cellMinY + collisionCellSize - from.y) / std::max(dy, 1e-4f)) : ((from.y - cellMinY) / std::max(dy, 1e-4f));

    while(true){
        const int idx = cellIndex(curX, curY);
        if(idx >= 0 && idx < static_cast<int>(gridBlocking.size())){
            if(!visitRanges(blockingRanges,idx,gridBlocking[idx],overlaps,testTriangle)) return false;
        }
        if(curX == endX && curY == endY) break;
        if(tMaxX < tMaxY){ tMaxX += tDeltaX; curX += stepX; }
        else { tMaxY += tDeltaY; curY += stepY; }
    }

    return visitRanges(blockingRanges,gridBlocking.size(),globalBlocking,overlaps,testTriangle);
}

bool Map::navigationSegmentClear(Vec3 from,Vec3 to,float radius,float height,float stepHeight) const{
    const Vec3 horizontal{to.x-from.x,to.y-from.y,0};
    const float distance=length(horizontal);
    if(distance<1.0f)return true;
    const int sampleCount = std::clamp(static_cast<int>(std::ceil(distance / std::max(32.0f, radius * 2.0f))), 1, 4);
    constexpr float noGround=-std::numeric_limits<float>::max()*0.25f;
    float prevGround = from.z;
    for(int s = 1; s <= sampleCount; ++s){
        const float t = static_cast<float>(s) / sampleCount;
        const float px = from.x + horizontal.x * t;
        const float py = from.y + horizontal.y * t;
        const float g = groundHeight(px, py, prevGround + stepHeight + 4.0f, noGround);
        if(g <= noGround * 0.5f || std::abs(g - prevGround) > stepHeight + 6.0f) return false;
        prevGround = g;
    }

    const Vec3 dir = horizontal / distance;
    const Vec3 lateral{-dir.y, dir.x, 0};
    const float shoulder = std::max(4.0f, radius * 0.70f);
    const float probeZ = std::max(12.0f, height * 0.40f);

    if(!lineOfSight(from + Vec3{0,0,probeZ}, to + Vec3{0,0,probeZ})) return false;
    if(!lineOfSight(from + lateral * shoulder + Vec3{0,0,probeZ}, to + lateral * shoulder + Vec3{0,0,probeZ})) return false;
    if(!lineOfSight(from - lateral * shoulder + Vec3{0,0,probeZ}, to - lateral * shoulder + Vec3{0,0,probeZ})) return false;
    return true;
}

Vec3 Map::constrainMove(Vec3 oldPosition,Vec3 proposed,float radius,float height,float stepHeight,Vec3* outContactNormal) const{
    constexpr float noGround=-std::numeric_limits<float>::max()*0.25f;
    const float sampledOldGround=navigationGroundHeight(oldPosition.x,oldPosition.y,oldPosition.z+stepHeight,noGround,radius*0.55f);
    const bool hasOldGround=sampledOldGround>noGround*0.5f;
    const float oldGround=hasOldGround?sampledOldGround:oldPosition.z;
    const bool grounded=hasOldGround&&(oldPosition.z-oldGround)<=stepHeight*1.5f&&(oldPosition.z-oldGround)>=-stepHeight*0.5f;

    Vec3 contactNormal{0.0f, 0.0f, 0.0f};
    bool hasContact = false;

    const auto resolveWalls=[&](Vec3 position){
        for(int iteration=0;iteration<3;++iteration){
            uint32_t epoch = ++queryEpoch;
            if(epoch == 0){
                std::fill(triangleTag.begin(), triangleTag.end(), 0);
                epoch = queryEpoch = 1;
            }
            const int minX=gridCoordX(position.x-radius),maxX=gridCoordX(position.x+radius);
            const int minY=gridCoordY(position.y-radius),maxY=gridCoordY(position.y+radius);
            bool changed=false;

            const auto testTriangle=[&](std::uint32_t index){
                if(triangleTag[index] == epoch) return;
                triangleTag[index] = epoch;
                const auto& triangle=collision[index];
                if(!triangle.blocking||(grounded&&triangle.maximum.z<=oldGround+stepHeight*1.25f)||position.x+radius<triangle.minimum.x||position.x-radius>triangle.maximum.x||position.y+radius<triangle.minimum.y||position.y-radius>triangle.maximum.y||position.z+height<triangle.minimum.z||position.z>triangle.maximum.z)return;
                const std::array<float,3> samples={radius,std::max(radius,height*0.5f),std::max(radius,height-radius)};
                for(const float sample:samples){
                    const Vec3 center=position+Vec3{0,0,sample};
                    const Vec3 nearest=closestPoint(center,triangle.a,triangle.b,triangle.c);
                    const Vec3 delta=center-nearest;
                    const float distSq=dot(delta,delta);
                    if(distSq>=radius*radius)continue;
                    const float dist=std::sqrt(std::max(1e-8f,distSq));
                    const float penetration=radius-dist;
                    const Vec3 normalOut=dist>1e-4f?(delta/dist):(length(triangle.normal)>0.1f?triangle.normal:Vec3{0,0,1});
                    position=position+normalOut*(penetration+0.02f);
                    contactNormal=(length(triangle.normal)>0.1f)?(dot(normalOut,triangle.normal)>=0.0f?triangle.normal:triangle.normal*-1.0f):normalOut;
                    hasContact=true;
                    changed=true;
                }
            };

            // Re-evaluate against the CURRENT pushed position at every node.
            // When cells overlap, stamp skipped triangles too: the old solver
            // visited/rejected them and must not reconsider them in a later cell.
            const auto overlaps=[&](const auto& node){return !((grounded&&node.maximum.z<=oldGround+stepHeight*1.25f)||position.x+radius<node.minimum.x||position.x-radius>node.maximum.x||position.y+radius<node.minimum.y||position.y-radius>node.maximum.y||position.z+height<node.minimum.z||position.z>node.maximum.z);};
            const auto visit=[&](std::uint32_t index){testTriangle(index);return true;};
            for(int y=minY;y<=maxY;++y){
                for(int x=minX;x<=maxX;++x){
                    const int idx = cellIndex(x, y);
                    if(idx >= 0 && idx < static_cast<int>(gridBlocking.size())){
                        const auto& list=gridBlocking[idx];
                        visitRanges(blockingRanges,idx,list,overlaps,visit,[&](auto begin,auto end){if(minX!=maxX||minY!=maxY)for(auto j=begin;j<end;++j)triangleTag[list[j]]=epoch;});
                    }
                }
            }
            visitRanges(blockingRanges,gridBlocking.size(),globalBlocking,overlaps,visit);
            if(!changed)break;
        }
        return position;
    };

    Vec3 direct=resolveWalls(proposed);
    if(!grounded||stepHeight<=0){
        if(outContactNormal&&hasContact)*outContactNormal=contactNormal;
        return direct;
    }

    const float directGround=navigationGroundHeight(direct.x,direct.y,std::max(oldPosition.z,direct.z)+stepHeight*1.5f,oldGround,radius*0.55f);
    const float directRise=directGround-oldGround;
    bool directBlocked=directRise>stepHeight*1.25f;
    if(directBlocked){
        direct.x=oldPosition.x;
        direct.y=oldPosition.y;
    }

    Vec3 stepped=proposed;
    stepped.z=oldGround+stepHeight*1.25f;
    stepped=resolveWalls(stepped);
    const float steppedGround=navigationGroundHeight(stepped.x,stepped.y,oldGround+stepHeight*1.5f,oldGround,radius*0.55f);
    const float stepRise=steppedGround-oldGround;
    if(stepRise>stepHeight*1.25f||stepRise<-stepHeight*4.0f){
        if(outContactNormal&&hasContact)*outContactNormal=contactNormal;
        return directBlocked?Vec3{oldPosition.x,oldPosition.y,oldPosition.z}:direct;
    }
    stepped.z=steppedGround;
    const float traceZ = std::max(stepHeight + 2.0f, height * 0.4f);
    if(!lineOfSight(oldPosition + Vec3{0, 0, traceZ}, stepped + Vec3{0, 0, traceZ})){
        if(outContactNormal&&hasContact)*outContactNormal=contactNormal;
        return directBlocked?Vec3{oldPosition.x,oldPosition.y,oldPosition.z}:direct;
    }
    const Vec2 directTravel{direct.x-oldPosition.x,direct.y-oldPosition.y},stepTravel{stepped.x-oldPosition.x,stepped.y-oldPosition.y};
    const float directDistanceSquared=directTravel.x*directTravel.x+directTravel.y*directTravel.y,stepDistanceSquared=stepTravel.x*stepTravel.x+stepTravel.y*stepTravel.y;
    const Vec3 chosen=(directBlocked||stepDistanceSquared>directDistanceSquared+0.01f)?stepped:direct;
    if(outContactNormal&&hasContact)*outContactNormal=contactNormal;
    return chosen;
}

Map::SurfContact Map::findSurfContact(Vec3 position, float radius, float height, float searchMargin) const {
    SurfContact result;
    float bestSeparation = searchMargin;

    const std::array<float, 3> samples = {radius, std::max(radius, height * 0.5f), std::max(radius, height - radius)};
    const float maxDist = radius + searchMargin;
    const float maxDistSq = maxDist * maxDist;
    const auto overlaps=[&](const auto& node){return !(position.x+maxDist<node.minimum.x||position.x-maxDist>node.maximum.x||position.y+maxDist<node.minimum.y||position.y-maxDist>node.maximum.y||position.z+height<node.minimum.z||position.z-searchMargin>node.maximum.z);};

    uint32_t epoch = ++queryEpoch;
    if (epoch == 0) {
        std::fill(triangleTag.begin(), triangleTag.end(), 0);
        epoch = queryEpoch = 1;
    }

    const int minX = gridCoordX(position.x - maxDist), maxX = gridCoordX(position.x + maxDist);
    const int minY = gridCoordY(position.y - maxDist), maxY = gridCoordY(position.y + maxDist);

    const auto testTriangle = [&](std::uint32_t index) {
        if (triangleTag[index] == epoch) return;
        triangleTag[index] = epoch;
        const auto& triangle = collision[index];
        // Only consider surf ramp triangles (steep, non-ground slope)
        if (!triangle.blocking || triangle.normal.z >= 0.70f || triangle.normal.z < 0.01f) return;
        if (position.x + maxDist < triangle.minimum.x || position.x - maxDist > triangle.maximum.x ||
            position.y + maxDist < triangle.minimum.y || position.y - maxDist > triangle.maximum.y ||
            position.z + height < triangle.minimum.z || position.z - searchMargin > triangle.maximum.z) return;

        for (const float sample : samples) {
            const Vec3 center = position + Vec3{0, 0, sample};
            const Vec3 nearest = closestPoint(center, triangle.a, triangle.b, triangle.c);
            const Vec3 delta = center - nearest;
            const float distSq = dot(delta, delta);
            if (distSq >= maxDistSq) continue;
            const float dist = std::sqrt(std::max(1e-8f, distSq));
            const float sep = dist - radius;
            if (sep < bestSeparation) {
                bestSeparation = sep;
                result.normal = triangle.normal;
                result.distance = dist;
                result.separation = sep;
                result.hit = true;
            }
        }
    };

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const int idx = cellIndex(x, y);
            if (idx >= 0 && idx < static_cast<int>(gridBlocking.size())) {
                visitRanges(blockingRanges,idx,gridBlocking[idx],overlaps,[&](auto index){testTriangle(index);return true;});
            }
        }
    }
    visitRanges(blockingRanges,gridBlocking.size(),globalBlocking,overlaps,[&](auto index){testTriangle(index);return true;});

    return result;
}

std::optional<Vec3> Map::mantleTarget(Vec3 position,Vec3 forward,float radius,float height,float stepHeight,float maxHeight,float checkRange,float minHeight,bool useAuthored) const{
    forward.z=0;if(length(forward)<0.5f)return std::nullopt;forward=normalize(forward);
    if(useAuthored)for(const auto direction:gameplay.mantleDirections(position,forward,radius,height,checkRange))
        if(const auto target=mantleTarget(position,direction,radius,height,stepHeight,maxHeight,checkRange,minHeight,false))return target;
    constexpr float noGround=-std::numeric_limits<float>::max()*0.25f;
    const float currentGround = navigationGroundHeight(position.x, position.y, position.z + stepHeight, noGround, radius * 0.45f);
    const bool isAirborne = (currentGround > noGround * 0.5f) && (position.z > currentGround + 4.0f);

    const float probeDistance=radius+checkRange;
    bool frontBlocked=false;
    for(int sample=1;sample<=8&&!frontBlocked;++sample){
        const float distance=probeDistance*(static_cast<float>(sample)/8.0f);
        const Vec3 result=constrainMove(position,position+forward*distance,radius,height,stepHeight);
        frontBlocked=dot(result-position,forward)<distance-1.0f;
    }
    if(!frontBlocked && isAirborne && currentGround > noGround * 0.5f){
        const Vec3 lowerPos{position.x, position.y, currentGround + stepHeight * 1.5f};
        for(int sample=1;sample<=8&&!frontBlocked;++sample){
            const float distance=probeDistance*(static_cast<float>(sample)/8.0f);
            const Vec3 result=constrainMove(lowerPos,lowerPos+forward*distance,radius,height,stepHeight);
            frontBlocked=dot(result-lowerPos,forward)<distance-1.0f;
        }
    }
    if(!frontBlocked && isAirborne){
        frontBlocked = true;
    }
    if(!frontBlocked)return std::nullopt;
    const auto capsuleClear=[&](Vec3 target){
        uint32_t epoch = ++queryEpoch;
        if(epoch == 0){
            std::fill(triangleTag.begin(), triangleTag.end(), 0);
            epoch = queryEpoch = 1;
        }
        const int minX=gridCoordX(target.x-radius),maxX=gridCoordX(target.x+radius);
        const int minY=gridCoordY(target.y-radius),maxY=gridCoordY(target.y+radius);
        bool clear=true;
        const auto overlaps=[&](const auto& node){return !(node.maximum.z<=target.z+2.f||target.x+radius<node.minimum.x||target.x-radius>node.maximum.x||target.y+radius<node.minimum.y||target.y-radius>node.maximum.y||target.z+height<node.minimum.z||target.z>node.maximum.z);};

        const auto testTriangle = [&](std::uint32_t index) -> bool {
            if(triangleTag[index] == epoch) return true;
            triangleTag[index] = epoch;
            const auto& triangle=collision[index];
            if(!triangle.blocking||triangle.maximum.z<=target.z+2.0f||target.x+radius<triangle.minimum.x||target.x-radius>triangle.maximum.x||target.y+radius<triangle.minimum.y||target.y-radius>triangle.maximum.y||target.z+height<triangle.minimum.z||target.z>triangle.maximum.z)return true;
            for(const float sample:{radius,height*0.5f,std::max(radius,height-radius)}){
                if(length(target+Vec3{0,0,sample}-closestPoint(target+Vec3{0,0,sample},triangle.a,triangle.b,triangle.c))<radius*0.92f)return false;
            }
            return true;
        };

        for(int y=minY;y<=maxY&&clear;++y){
            for(int x=minX;x<=maxX&&clear;++x){
                const int idx = cellIndex(x, y);
                if(idx >= 0 && idx < static_cast<int>(gridBlocking.size())){
                    clear=visitRanges(blockingRanges,idx,gridBlocking[idx],overlaps,testTriangle);
                }
            }
        }
        if(clear)clear=visitRanges(blockingRanges,gridBlocking.size(),globalBlocking,overlaps,testTriangle);
        return clear;
    };
    for(int sample=1;sample<=4;++sample){
        const float distance=radius+checkRange*(static_cast<float>(sample)/4.0f);
        Vec3 target=position+forward*distance;
        const float top=navigationGroundHeight(target.x,target.y,position.z+maxHeight,noGround,radius*0.70f);
        if(top<=noGround*0.5f)continue;
        const float rise=top-position.z;
        // Minimum rise is measured from the current feet position for both
        // grounded and airborne acquisition. Omitted preserves legacy policy.
        if(rise<minHeight)continue;
        if(isAirborne){
            if(rise < -height * 0.45f || rise > maxHeight) continue;
            if(currentGround > noGround * 0.5f && top <= currentGround + stepHeight + 2.0f) continue;
        }else{
            if(rise<=stepHeight+4.0f||rise>maxHeight)continue;
        }
        target.z=top;
        if(capsuleClear(target))return target;
    }
    return std::nullopt;
}

bool Map::isBounceSurface(float x, float y, float z, float radius) const {
    const int minX = gridCoordX(x - radius), maxX = gridCoordX(x + radius);
    const int minY = gridCoordY(y - radius), maxY = gridCoordY(y + radius);
    const auto testTriangle = [&](std::uint32_t index) -> bool {
        const auto& tri = collision[index];
        if (!tri.bounce) return false;
        if (x + radius < tri.minimum.x || x - radius > tri.maximum.x ||
            y + radius < tri.minimum.y || y - radius > tri.maximum.y) return false;
        if (z < tri.minimum.z - 30.0f || z > tri.maximum.z + 45.0f) return false;
        const Vec3 p{x, y, std::clamp(z, tri.minimum.z, tri.maximum.z)};
        return length(p - closestPoint(p, tri.a, tri.b, tri.c)) <= radius + 4.0f;
    };
    for (int cy = minY; cy <= maxY; ++cy) {
        for (int cx = minX; cx <= maxX; ++cx) {
            const int idx = cellIndex(cx, cy);
            if (idx >= 0 && idx < static_cast<int>(gridCollision.size())) {
                for (const auto index : gridCollision[idx]) {
                    if (testTriangle(index)) return true;
                }
            }
        }
    }
    for (const auto index : globalCollision) {
        if (testTriangle(index)) return true;
    }
    return false;
}

int Map::speedBoostTier(float x, float y, float z, float radius) const {
    const int minX = gridCoordX(x - radius), maxX = gridCoordX(x + radius);
    const int minY = gridCoordY(y - radius), maxY = gridCoordY(y + radius);
    int tier = 0;
    const auto testTriangle = [&](std::uint32_t index) {
        const auto& tri = collision[index];
        if (!tri.speedboost && !tri.speedboost2) return;
        if (x + radius < tri.minimum.x || x - radius > tri.maximum.x ||
            y + radius < tri.minimum.y || y - radius > tri.maximum.y) return;
        if (z < tri.minimum.z - 30.0f || z > tri.maximum.z + 45.0f) return;
        const Vec3 p{x, y, std::clamp(z, tri.minimum.z, tri.maximum.z)};
        if (length(p - closestPoint(p, tri.a, tri.b, tri.c)) <= radius + 4.0f) {
            if (tri.speedboost2) tier = std::max(tier, 2);
            else if (tri.speedboost) tier = std::max(tier, 1);
        }
    };
    for (int cy = minY; cy <= maxY; ++cy) {
        for (int cx = minX; cx <= maxX; ++cx) {
            const int idx = cellIndex(cx, cy);
            if (idx >= 0 && idx < static_cast<int>(gridCollision.size())) {
                for (const auto index : gridCollision[idx]) {
                    testTriangle(index);
                    if (tier >= 2) return 2;
                }
            }
        }
    }
    for (const auto index : globalCollision) {
        testTriangle(index);
        if (tier >= 2) return 2;
    }
    return tier;
}

std::optional<Vec3> Map::findLadderContact(Vec3 position, float radius, float height) const {
    const int minX = gridCoordX(position.x - radius - 10.0f), maxX = gridCoordX(position.x + radius + 10.0f);
    const int minY = gridCoordY(position.y - radius - 10.0f), maxY = gridCoordY(position.y + radius + 10.0f);
    std::optional<Vec3> ladderNormal;
    float nearestDist = 1e9f;
    const auto testTriangle = [&](std::uint32_t index) {
        const auto& tri = collision[index];
        if (!tri.ladder) return;
        if (position.x + radius + 10.0f < tri.minimum.x || position.x - radius - 10.0f > tri.maximum.x ||
            position.y + radius + 10.0f < tri.minimum.y || position.y - radius - 10.0f > tri.maximum.y ||
            position.z + height + 5.0f < tri.minimum.z || position.z - 5.0f > tri.maximum.z) return;
        for (const float sample : {radius, height * 0.5f, height - radius}) {
            const Vec3 probe = position + Vec3{0, 0, sample};
            const Vec3 closest = closestPoint(probe, tri.a, tri.b, tri.c);
            const float dist = length(probe - closest);
            if (dist < radius + 12.0f && dist < nearestDist) {
                nearestDist = dist;
                ladderNormal = tri.normal;
            }
        }
    };
    for (int cy = minY; cy <= maxY; ++cy) {
        for (int cx = minX; cx <= maxX; ++cx) {
            const int idx = cellIndex(cx, cy);
            if (idx >= 0 && idx < static_cast<int>(gridCollision.size())) {
                for (const auto index : gridCollision[idx]) testTriangle(index);
            }
        }
    }
    for (const auto index : globalCollision) testTriangle(index);
    return ladderNormal;
}

bool load(const std::filesystem::path& path,Map& map,std::string& error,float scaleMultiplier,bool buildRenderCollision){
    error.clear();map={};map.scaleMultiplier=scaleMultiplier;cgltf_options options{};cgltf_data* data{};const auto utf8=path.u8string();const std::string filename(reinterpret_cast<const char*>(utf8.data()),utf8.size());auto result=cgltf_parse_file(&options,filename.c_str(),&data);if(result!=cgltf_result_success){error="Could not parse GLB (cgltf result "+std::to_string(static_cast<int>(result))+")";return false;}const auto cleanup=[&]{cgltf_free(data);};
    try{
        const auto validateUri=[&](const char* uri){if(uri&&uri[0]&&!std::string_view(uri).starts_with("data:"))codm::packageTexturePath(path.parent_path(),uri);};
        for(cgltf_size i=0;i<data->images_count;++i)validateUri(data->images[i].uri);
        for(cgltf_size i=0;i<data->buffers_count;++i)validateUri(data->buffers[i].uri);
    }catch(const std::exception& e){cleanup();error=std::string("Unsafe GLB package resource: ")+e.what();return false;}
    result=cgltf_load_buffers(&options,data,filename.c_str());if(result!=cgltf_result_success){cleanup();error="Could not load GLB buffers";return false;}if(cgltf_validate(data)!=cgltf_result_success){cleanup();error="GLB validation failed";return false;}
    std::error_code ec;const auto cache=std::filesystem::temp_directory_path(ec)/"CastStage"/"glb_cache"/path.stem();std::unordered_map<const cgltf_image*,std::filesystem::path> extracted;std::unordered_map<const cgltf_material*,std::size_t> materialMeshes;
    std::optional<Vec3> authoredSpawn;
    for(cgltf_size nodeIndex=0;nodeIndex<data->nodes_count;++nodeIndex){const auto& node=data->nodes[nodeIndex];
        const std::string rawName = node.name ? node.name : (node.mesh && node.mesh->name ? node.mesh->name : "");
        const std::string cName = canonical(rawName);
        if(!authoredSpawn && (cName.find("spawn") != std::string::npos || cName.find("player_start") != std::string::npos || cName.find("info_player_start") != std::string::npos || cName == "player_spawn")){
            const Mat4 world = nodeMatrix(&node, scaleMultiplier);
            authoredSpawn = Vec3{world.v[12], world.v[13], world.v[14]};
        }
        if(!node.mesh)continue;const Mat4 world=nodeMatrix(&node,scaleMultiplier);for(cgltf_size primitiveIndex=0;primitiveIndex<node.mesh->primitives_count;++primitiveIndex){const auto& primitive=node.mesh->primitives[primitiveIndex];if(primitive.type!=cgltf_primitive_type_triangles)continue;const cgltf_accessor *positions{},*normals{},*uvs{},*colors{};for(cgltf_size attributeIndex=0;attributeIndex<primitive.attributes_count;++attributeIndex){const auto& attribute=primitive.attributes[attributeIndex];if(attribute.type==cgltf_attribute_type_position)positions=attribute.data;else if(attribute.type==cgltf_attribute_type_normal)normals=attribute.data;else if(attribute.type==cgltf_attribute_type_texcoord&&attribute.index==0)uvs=attribute.data;else if(attribute.type==cgltf_attribute_type_color&&attribute.index==0)colors=attribute.data;}if(!positions||positions->count==0)continue;
            std::string nodeName=node.name?node.name:(node.mesh->name?node.mesh->name:"mesh");const std::string canonicalName=canonical(nodeName);const std::string materialName=primitive.material&&primitive.material->name?primitive.material->name:"";const std::string canonicalMaterial=canonical(materialName);
            Mesh mesh;
            if(canonicalMaterial=="sky"||canonicalMaterial.find("skybox")!=std::string::npos||canonicalMaterial.find("skydome")!=std::string::npos||canonicalName=="sky"||canonicalName.find("skybox")!=std::string::npos||canonicalName.find("skydome")!=std::string::npos)continue;
            mesh.name =
                "GLB / " +
                std::string(materialName.empty() ? nodeName : materialName);
            mesh.materialName = materialName;
            mesh.skinned = false;
            mesh.decal = isDecalSurface(canonicalName) ||
                         isDecalSurface(canonicalMaterial);
            mesh.forceAlpha =
                primitive.material &&
                primitive.material->alpha_mode == cgltf_alpha_mode_blend;
            mesh.alphaTest =
                primitive.material &&
                primitive.material->alpha_mode == cgltf_alpha_mode_mask;
            mesh.materialPolicyExplicit=true;mesh.useVertexColor=true;mesh.color={1,1,1,1};mesh.normalProfile=3;
            if (primitive.material) {
              mesh.doubleSided=primitive.material->double_sided;
              mesh.alphaCutoff=primitive.material->alpha_cutoff;
              mesh.ignoreAlbedoAlpha=primitive.material->alpha_mode==cgltf_alpha_mode_opaque;
              mesh.unlit=primitive.material->unlit;
              if(primitive.material->extras.data){
                try{
                  const auto extras=codm::parseJson(primitive.material->extras.data);
                  mesh.sourceMaterialMetadata=std::make_shared<const std::string>(extras.dump());
                  if(extras.value("sky",false))continue;
                  if(extras.contains("codm")){
                    const auto& codm=extras.at("codm");
                    if(codm.value("sky",false))continue;
                    mesh.decal=codm.value("decal",false);
                    // The compiler already encoded additive as GLB alpha preview.
                    // Do not reinterpret this approximation as a second additive pass.
                    mesh.decalAdditive=false;mesh.decalMultiply=false;
                  }
                }catch(const std::exception& e){cleanup();error=std::string("Invalid GLB material extras: ")+e.what();return false;}
              }
              if (primitive.material->has_pbr_metallic_roughness) {
                mesh.gltfPbr = true;
                mesh.albedoPath =
                    materialTexture(&primitive.material->pbr_metallic_roughness
                                         .base_color_texture,
                                    path, cache, extracted);
                mesh.specularPath =
                    materialTexture(&primitive.material->pbr_metallic_roughness
                                         .metallic_roughness_texture,
                                    path, cache, extracted);
                const auto &pbr = primitive.material->pbr_metallic_roughness;
                const auto &factor = pbr.base_color_factor;
                mesh.color = {factor[0], factor[1], factor[2], factor[3]};
                mesh.metallicFactor = pbr.metallic_factor;
                mesh.roughnessFactor = pbr.roughness_factor;
              }
              mesh.normalPath = materialTexture(
                  &primitive.material->normal_texture, path, cache, extracted);
              if (primitive.material->has_specular && !mesh.gltfPbr)
                if (const auto extension = materialTexture(
                        &primitive.material->specular.specular_texture, path,
                        cache, extracted);
                    !extension.empty())
                  mesh.specularPath = extension;
              if (primitive.material->has_transmission)
                mesh.transmissionFactor =
                    primitive.material->transmission.transmission_factor;
              if (primitive.material->has_ior)
                mesh.indexOfRefraction = primitive.material->ior.ior;
              mesh.emissiveFactor = {primitive.material->emissive_factor[0],
                                     primitive.material->emissive_factor[1],
                                     primitive.material->emissive_factor[2]};
              std::filesystem::path emissiveTexturePath;
              if (primitive.material->emissive_texture.texture) {
                emissiveTexturePath = materialTexture(
                    &primitive.material->emissive_texture, path, cache, extracted);
              }
              mesh.emissivePath=emissiveTexturePath;
              mesh.emissive=false; // Emission is a separate additive radiance term, not diffuse replacement.
            }
            mesh.vertices.resize(positions->count);for(cgltf_size i=0;i<positions->count;++i){float value[4]{};cgltf_accessor_read_float(positions,i,value,3);auto& vertex=mesh.vertices[i];vertex.position=transformPoint(world,convertPoint(value,scaleMultiplier));if(normals&&i<normals->count){cgltf_accessor_read_float(normals,i,value,3);vertex.normal=transformVector(world,convertVector(value));}if(uvs&&i<uvs->count){cgltf_accessor_read_float(uvs,i,value,2);vertex.uv={value[0],value[1]};}if(colors&&i<colors->count){value[0]=value[1]=value[2]=value[3]=1;cgltf_accessor_read_float(colors,i,value,4);vertex.color={value[0],value[1],value[2],value[3]};}growBounds(map.scene.bounds,vertex.position);}
            const cgltf_size indexCount=primitive.indices?primitive.indices->count:positions->count;mesh.indices.reserve(indexCount);for(cgltf_size i=0;i<indexCount;++i)mesh.indices.push_back(static_cast<std::uint32_t>(primitive.indices?cgltf_accessor_read_index(primitive.indices,i):i));
            Vec3 primitiveMinimum=mesh.vertices.front().position,primitiveMaximum=primitiveMinimum;for(const auto& vertex:mesh.vertices){primitiveMinimum={std::min(primitiveMinimum.x,vertex.position.x),std::min(primitiveMinimum.y,vertex.position.y),std::min(primitiveMinimum.z,vertex.position.z)};primitiveMaximum={std::max(primitiveMaximum.x,vertex.position.x),std::max(primitiveMaximum.y,vertex.position.y),std::max(primitiveMaximum.z,vertex.position.z)};}const float primitiveVerticalSpan=primitiveMaximum.z-primitiveMinimum.z,primitiveHorizontalSpan=std::max(primitiveMaximum.x-primitiveMinimum.x,primitiveMaximum.y-primitiveMinimum.y);
            const bool collidable=(!primitive.material||primitive.material->alpha_mode!=cgltf_alpha_mode_blend)&&!mesh.decal&&!canonicalName.starts_with("fx_")&&!canonicalName.starts_with("foliage_")&&!isNonSolidName(canonicalName)&&!isNonSolidName(canonicalMaterial);
            if(buildRenderCollision&&collidable)for(std::size_t i=0;i+2<mesh.indices.size();i+=3){const auto ia=mesh.indices[i],ib=mesh.indices[i+1],ic=mesh.indices[i+2];if(ia>=mesh.vertices.size()||ib>=mesh.vertices.size()||ic>=mesh.vertices.size())continue;CollisionTriangle triangle;triangle.a=mesh.vertices[ia].position;triangle.b=mesh.vertices[ib].position;triangle.c=mesh.vertices[ic].position;triangle.normal=normalize(cross(triangle.b-triangle.a,triangle.c-triangle.a));if(length(triangle.normal)<0.5f)continue;triangle.minimum={std::min({triangle.a.x,triangle.b.x,triangle.c.x}),std::min({triangle.a.y,triangle.b.y,triangle.c.y}),std::min({triangle.a.z,triangle.b.z,triangle.c.z})};triangle.maximum={std::max({triangle.a.x,triangle.b.x,triangle.c.x}),std::max({triangle.a.y,triangle.b.y,triangle.c.y}),std::max({triangle.a.z,triangle.b.z,triangle.c.z})};triangle.walkable=triangle.normal.z>=0.70f;triangle.blocking=triangle.normal.z<0.70f;
                triangle.bounce = canonicalName.find("bounce_") != std::string::npos || canonicalMaterial.find("bounce_") != std::string::npos;
                triangle.speedboost2 = canonicalName.find("speedboost2_") != std::string::npos || canonicalMaterial.find("speedboost2_") != std::string::npos;
                triangle.speedboost = !triangle.speedboost2 && (canonicalName.find("speedboost_") != std::string::npos || canonicalMaterial.find("speedboost_") != std::string::npos);
                triangle.ladder = canonicalName.find("ladder_") != std::string::npos || canonicalMaterial.find("ladder_") != std::string::npos;
                if(triangle.bounce) triangle.walkable = true;
                if(triangle.walkable||triangle.blocking||triangle.bounce||triangle.ladder)map.collision.push_back(triangle);}
            if(const auto found=materialMeshes.find(primitive.material);found!=materialMeshes.end()){auto& destination=map.scene.meshes[found->second];destination.decal=destination.decal||mesh.decal;const auto vertexOffset=static_cast<std::uint32_t>(destination.vertices.size());destination.vertices.insert(destination.vertices.end(),mesh.vertices.begin(),mesh.vertices.end());destination.indices.reserve(destination.indices.size()+mesh.indices.size());for(const auto index:mesh.indices)destination.indices.push_back(vertexOffset+index);}
            else{materialMeshes.emplace(primitive.material,map.scene.meshes.size());map.scene.meshes.push_back(std::move(mesh));}
        }}cleanup();if(map.scene.meshes.empty()){error="GLB contains no triangle meshes";return false;}if(!buildRenderCollision){map.sourcePath=path;return true;}map.buildCollisionIndex();
        if(authoredSpawn){
            constexpr float noGround = -1e9f;
            const float ground = map.groundHeight(authoredSpawn->x, authoredSpawn->y, authoredSpawn->z + 150.0f, noGround);
            if(ground > noGround * 0.5f){
                map.defaultSpawnPoint = {authoredSpawn->x, authoredSpawn->y, ground + 5.0f};
            }else{
                map.defaultSpawnPoint = *authoredSpawn;
            }
            map.hasDefaultSpawnPoint = true;
        }else if(const auto spawn=findFallbackSpawn(map)){
            map.defaultSpawnPoint=*spawn;map.hasDefaultSpawnPoint=true;
        }
        map.sourcePath=path;return true;
}

} // namespace scene::glb
