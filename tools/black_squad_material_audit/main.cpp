#include "source_resolver.hpp"
#include "property_tags.hpp"
#include <iostream>
#include <set>
#include <cstring>
int main(int argc,char** argv){
    if(argc!=3)return 2;const std::filesystem::path game=argv[1],assets=argv[2];
    const auto pad=bs::find_summary_pad(game/"binaries/win64/BlackSquadGame.exe");
    auto catalog=bs::build_source_catalog(game/"CombatGame/CookedPC",pad);bs::SourceResolver resolver(catalog,pad);
    std::set<std::string> packages,seen;int failed{};
    for(const auto& f:std::filesystem::directory_iterator(assets/"models"))if(f.path().extension()==".cast"){
        const auto n=f.path().stem().string();const auto start=n.rfind("__"),end=n.rfind('_');
        if(start!=n.npos&&end>start+2)packages.insert(n.substr(start+2,end-start-2));
    }
    for(const auto& name:packages)try{
        auto p=bs::read_package_index(game/"CombatGame/CookedPC"/(name+".upk"),pad);
        for(std::size_t i=0;i<p.exports.size();++i)if(p.class_name(p.exports[i])=="MaterialInstanceConstant"){
            bs::ResolvedObject o{&p,i,{}};std::vector<std::shared_ptr<bs::PackageIndex>> owners;
            const auto mat=p.name(p.exports[i].object_name);if(!seen.insert(mat).second)continue;
            for(int depth=0;depth<32;++depth){
                const auto& e=o.package->exports[o.index];const auto kind=o.package->class_name(e);
                if(kind=="Material"){std::cout<<mat<<'\t'<<o.package->name(e.object_name)<<'\t'<<o.package->path.filename().string()<<'\t'<<o.index+1<<'\n';break;}
                const auto b=bs::read_export_payload(*o.package,e);int parent{};
                for(const auto& t:bs::scan_ue3_properties(*o.package,b).tags)if(t.name=="Parent"&&t.value_size==4)std::memcpy(&parent,b.data()+t.value_offset,4);
                if(!parent){std::cout<<mat<<"\tUNRESOLVED\n";++failed;break;}
                o=resolver.resolve(*o.package,parent);if(o.owner)owners.push_back(o.owner);
            }
        }
    }catch(const std::exception& e){std::cerr<<name<<": "<<e.what()<<'\n';++failed;}
    std::cerr<<"Packages "<<packages.size()<<", materials "<<seen.size()<<", failures "<<failed<<'\n';return failed?1:0;
}
