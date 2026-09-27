#include "assets/ImportedGamePolicy.h"
#include "scene/CastScene.h"
#include "scene/ImportedNative.h"
#include <iostream>
#include <set>
#include <cmath>
int main(int argc,char** argv){
    if(argc<2)return 2;
    if(std::string_view(argv[1])=="--inspect"&&argc>2){
        const auto doc=cast::Document::load(argv[2]);auto s=scene::buildScene(doc,false);
        for(const auto& m:s.meshes)std::cout<<"MESH "<<m.name<<" vertices="<<m.vertices.size()<<" texture="<<assets::imported::utf8(m.albedoPath)<<'\n';
        for(const auto& b:s.skeleton.bones)std::cout<<"BONE "<<b.name<<" parent="<<b.parent<<" pos="<<b.restGlobal.v[12]<<','<<b.restGlobal.v[13]<<','<<b.restGlobal.v[14]<<'\n';
        for(const auto& a:s.animations)std::cout<<"ANIM "<<a.name<<" source="<<a.sourceName<<" tracks="<<a.tracks.size()<<" unmapped="<<a.unmappedCurveCount<<'\n';
        for(const auto& root:doc.roots())for(const auto& node:root.children)for(const auto& p:node.properties)if(p.stringValue)std::cout<<"PROPERTY "<<p.name<<'='<<*p.stringValue<<'\n';
        return doc.valid()?0:1;
    }
    const std::filesystem::path root=argv[1];std::size_t failures{};
    std::cout<<"game,model,role,meshes,bones,height,clips,unmapped_curves,missing_textures,nonfinite_poses,nonfinite_uvs,tiled_uv_vertices,degenerate_uv_triangles\n";
    for(const auto game:{"eldewrito","cs1.6","cz","css","csnz","cso2"}){
        if(argc>2&&std::string_view(argv[2])!=game)continue;
        std::vector<std::filesystem::path> clips;
        for(const auto& e:std::filesystem::recursive_directory_iterator(root/game/"animations"))if(e.path().extension()==".cast")clips.push_back(e.path());
        assets::Catalog catalog;std::string scanError;
        if(!assets::appendScan(root/game,game,catalog,scanError)){std::cerr<<scanError<<'\n';++failures;continue;}
        for(const auto& asset:catalog.entries){
            const auto& name=asset.name;const auto role=asset.role;
            std::cerr<<"Loading "<<game<<'/'<<name<<'\n';
            try{
            auto s=scene::buildScene(cast::Document::load(asset.path),false);
            if(role==assets::Role::ViewWeapon){
                const auto stem=assets::imported::weaponStem(game,name);const auto expected=stem+(std::string_view(game)=="cso2"?"_hand_707":"_hands");
                for(const auto& candidate:catalog.entries)if(candidate.role==assets::Role::ViewHands&&candidate.name==expected){std::string error;if(!scene::imported::assemble(cast::Document::load(asset.path),cast::Document::load(candidate.path),s,error)){std::cerr<<error<<'\n';++failures;}break;}
            }
            float lo=1e9f,hi=-1e9f;std::size_t missing{},unmapped{},bad{},badUv{},tiled{},degenerate{};
            for(const auto& m:s.meshes){for(const auto& v:m.vertices){lo=std::min(lo,v.position.z);hi=std::max(hi,v.position.z);}if(!m.albedoPath.empty()&&!std::filesystem::is_regular_file(m.albedoPath))++missing;}
            for(const auto& m:s.meshes){
                for(const auto& v:m.vertices){badUv+=!std::isfinite(v.uv.x)||!std::isfinite(v.uv.y);tiled+=v.uv.x<0||v.uv.x>1||v.uv.y<0||v.uv.y>1;}
                for(std::size_t i=0;i+2<m.indices.size();i+=3){const auto a=m.vertices[m.indices[i]].uv,b=m.vertices[m.indices[i+1]].uv,c=m.vertices[m.indices[i+2]].uv;degenerate+=std::abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))<1e-12f;}
            }
            if(role==assets::Role::ViewWeapon)for(const auto& clip:clips)if(assets::imported::matchesAnimation(game,name,assets::imported::utf8(clip.filename()))){std::cerr<<"  clip "<<assets::imported::utf8(clip.filename())<<'\n';scene::appendAnimations(cast::Document::load(clip),s);}
            if(role==assets::Role::PlayerModel&&std::string_view(game)=="eldewrito")for(const auto& clip:clips)if(!assets::imported::dewBodySemantic(assets::imported::utf8(clip.filename())).empty())scene::appendAnimations(cast::Document::load(clip),s);
            for(std::size_t i=0;i<s.animations.size();++i){const auto& a=s.animations[i];unmapped+=a.unmappedCurveCount;for(const float t:{0.f,.37f,1.f})for(const auto& m:s.samplePose(i,a.durationFrames*t))for(const auto v:m.v)bad+=!std::isfinite(v);}
            std::cout<<game<<','<<name<<','<<assets::roleName(role)<<','<<s.meshes.size()<<','<<s.skeleton.bones.size()<<','<<(hi-lo)<<','<<s.animations.size()<<','<<unmapped<<','<<missing<<','<<bad<<','<<badUv<<','<<tiled<<','<<degenerate<<'\n';
            failures+=bad>0||badUv>0;
            }catch(const std::exception& error){std::cerr<<"FAILED "<<name<<": "<<error.what()<<'\n';++failures;}
        }
    }
    return failures?1:0;
}
