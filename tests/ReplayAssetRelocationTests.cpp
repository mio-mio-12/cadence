#include "app/ReplayAssetRelocation.h"
#include <chrono>
#include <fstream>
#include <iostream>
#define CHECK(x) do{if(!(x))throw std::runtime_error("failed line "+std::to_string(__LINE__));}while(false)
int main(){
 namespace fs=std::filesystem;using namespace cadence;
 const auto root=fs::temp_directory_path()/("cadence-relocation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 const auto write=[&](const fs::path& p){fs::create_directories(p.parent_path());std::ofstream(p)<<"fixture";};
 try {
  const auto css=root/"exported_files/css/models/shared.cast",csnz=root/"exported_files/csnz/models/shared.cast";
  write(css);write(csnz);
  assets::Catalog catalog;catalog.entries={{css,"shared","css"},{csnz,"shared","csnz"}};
  CHECK(replay::storedGame({}).empty());
  CHECK(replay::storedGame(fs::path("relative/shared.cast")).empty());
  CHECK(replay::storedGame(fs::path("Z:/_files/css")).empty());
  CHECK(replay::storedGame(fs::path("Z:/_files/CSS/models/shared.cast"))=="css");
  CHECK(!replay::relocateModel("",catalog,{}));
  const auto old="Z:/missing/exported_files/css/models/shared.cast";
  CHECK(replay::relocateModel(old,catalog,{})==fs::weakly_canonical(css));
  CHECK(!replay::relocateModel("Z:/legacy/shared.cast",catalog,{}));
  CHECK(!replay::relocateModel("Z:/missing/exported_files/bo2/models/shared.cast",catalog,{}));
  catalog.entries.push_back(catalog.entries.front());
  CHECK(replay::relocateModel(old,catalog,{})==fs::weakly_canonical(css));
  const auto alias=root/"alias/shared.cast";fs::create_directories(alias.parent_path());
  std::error_code linkError;fs::create_hard_link(css,alias,linkError);
  if(!linkError){catalog.entries.push_back({alias,"shared","css"});CHECK(replay::relocateModel(old,catalog,{})==fs::weakly_canonical(css));}
  const auto other=root/"alternative/shared.cast";write(other);catalog.entries.push_back({other,"shared","css"});
  CHECK(!replay::relocateModel(old,catalog,{}));
  CHECK(replay::relocateModel(content::utf8(css),catalog,{})==css);
  catalog.entries={{css,"shared","css"}};
  CHECK(replay::relocateModel("Z:/legacy/shared.cast",catalog,{})==fs::weakly_canonical(css));
  CHECK(replay::relocateModel("shared.cast",catalog,{})==fs::weakly_canonical(css));
  CHECK(replay::relocateModel("Z:/legacy/shared.old",catalog,{})==fs::weakly_canonical(css)); // existing stem fallback
  // Preserve the resolved catalog spelling; canonicalization is dedupe-only.
  const auto spelled=css.parent_path()/"."/css.filename();
  catalog.entries={{spelled,"shared","css"},{css,"shared","css"}};
  CHECK(replay::relocateModel(old,catalog,{})==spelled);
#ifdef _WIN32
  auto preferredAbsolute=fs::absolute(css).lexically_normal();
  preferredAbsolute.make_preferred(); // extended paths do not accept forward separators
  const fs::path extended(std::wstring(L"\\\\?\\")+preferredAbsolute.native());
  CHECK(fs::is_regular_file(extended));
  catalog.entries={{extended,"shared","css"},{css,"shared","css"}};
  CHECK(replay::relocateModel(old,catalog,{})==extended);
#endif
  catalog.entries.clear();
  CHECK(replay::relocateModel(old,catalog,root/"exported_files")==fs::weakly_canonical(css));
  CHECK(!replay::relocateModel("Z:/legacy/shared.cast",catalog,root/"exported_files"));
  content::setDisabledRoots({content::key(css.parent_path())});
  CHECK(!replay::relocateModel(content::utf8(css),catalog,root/"exported_files"));
  content::setDisabledRoots({});
  content::PathMount mounted;mounted.id="preferred";mounted.paths.emplace(content::key(fs::u8path(old)),csnz);
  content::mountPaths({mounted});content::preferPack("preferred");
  CHECK(replay::relocateModel(old,catalog,{})==csnz); // exact aliases still win
  content::mountPaths({});content::preferPack({});
  fs::remove_all(root);std::cout<<"PASS replay asset relocation: identity, ambiguity, exact mounts, disabled namespaces\n";
 }catch(const std::exception& e){content::setDisabledRoots({});content::mountPaths({});content::preferPack({});std::error_code ec;fs::remove_all(root,ec);std::cerr<<e.what()<<'\n';return 1;}
}
