#include "scene/T6SharedTextures.h"
#include <chrono>
#include <iostream>
int main(){namespace fs=std::filesystem;using scene::t6_shared_textures::Resolver;int failures=0;
 const auto check=[&](bool value,const char* name){if(!value){++failures;std::cerr<<name<<'\n';}};
 const auto root=fs::temp_directory_path()/("cadence_t6_textures_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 const auto source=root/"bo2_sp"/"models"/"actor.cast",missing=source.parent_path()/"gear.png";
 fs::create_directories(source.parent_path());fs::create_directories(root/"bo2"/"a");fs::create_directories(root/"bo2"/"b");
 const auto write=[](const fs::path& p,const char* data){std::ofstream out(p,std::ios::binary);out<<data;};
 write(root/"bo2"/"a"/"gear.png","same");write(root/"bo2"/"b"/"gear.png","same");
 write(root/"bo2"/"a"/"ambiguous.png","a");write(root/"bo2"/"b"/"ambiguous.png","b");
 Resolver resolver;
 check(resolver.resolve(source,{})==fs::path{},"Preserve intentional empty cornea");
 check(resolver.resolve(source,missing)==root/"bo2"/"a"/"gear.png","Identical exact sibling match");
 check(resolver.resolve(source,source.parent_path()/"ambiguous.png")==source.parent_path()/"ambiguous.png","Reject different-content duplicates");
 check(resolver.resolve(root/"ghosts"/"models"/"actor.cast",missing)==missing,"No cross-game fallback");
 check(resolver.resolve(source,source.parent_path()/"absent.png")==source.parent_path()/"absent.png","No approximate substitution");
 write(missing,"original");check(resolver.resolve(source,missing)==missing,"Existing original takes precedence over cached fallback");
 const auto late=source.parent_path()/"late.png";write(root/"bo2"/"a"/"late.png","late");check(resolver.resolve(source,late)==late,"Index remains cached, no repeated directory scan");
 fs::remove_all(root);return failures?1:0;
}
