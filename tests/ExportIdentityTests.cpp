#include "content/AssetPaths.h"
#include "scene/ImportedNative.h"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<'\n';return 1;}}while(false)
int main(){using namespace cadence::content;
 CHECK(exportKey("D:/saluki/exported/_files/csnz/models/gun.cast")=="csnz/models/gun.cast");
 CHECK(exportKey("exported_files/futuregame/models/gun.cast")=="futuregame/models/gun.cast");
 CHECK(exportKey("D:/exported_files/tools/exported_files/csnz/models/gun.cast").empty());
 CHECK(exportKey("D:/_files/tools/exported_files/csnz/models/gun.cast").empty());
 CHECK(scene::imported::gameForPath("D:/exported_files/tools/exported_files/csnz/models/gun.cast").empty());
 CHECK(scene::imported::gameForPath("D:/csnz/exported_files/cso2/models/gun.cast")=="cso2");
 CHECK(scene::imported::gameForPath("D:/csnz/exported_files/bo2/models/gun.cast").empty());
 const fs::path root="D:/exported_files/tools/Cadence/packs/test";
 const auto target=root/"assets/exported_files/csnz/models/gun.cast";
 PathMount mount;mount.id="test";mount.root=root;mount.paths["game:csnz/models/gun.cast"]=target;
 mountPaths({mount});
 CHECK(exportKey(target)=="csnz/models/gun.cast");
 CHECK(scene::imported::gameForPath(target)=="csnz");
 CHECK(resolve("Z:/exported_files/csnz/models/gun.cast")==target);
 CHECK(exportKey(root/"assets/exported_files/futuregame/models/_files/piece.cast")=="futuregame/models/_files/piece.cast");
 CHECK(exportKey(root/"unlisted/file.cast").empty());
 const auto custom=root/"assets/files/custom.cast";
 mount.paths["game:cso2/models/custom.cast"]=custom;mountPaths({mount});
 CHECK(exportKey(custom)=="cso2/models/custom.cast");
 mount.paths["game:csnz/models/custom.cast"]=custom;mountPaths({mount});
 CHECK(exportKey(custom).empty()); // conflicting logical aliases are not guessed
 mountPaths({});
 setFallbackExportRoot("D:/exported_files/tools/exported_files");
 CHECK(exportKey("D:/exported_files/tools/exported_files/bo2/models/_files/piece.cast")=="bo2/models/_files/piece.cast");
 setFallbackExportRoot({});
 CHECK(scene::imported::gameForPath("D:/exported_files/cs2/models/gun.cast").empty()); // unsupported imported path remains excluded
 std::cout<<"PASS export identity: authoritative pack/root, legacy layouts, ambiguous raw paths, imported game boundaries\n";
}
