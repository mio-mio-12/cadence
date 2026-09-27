#include "cast/CastDocument.h"
#include "content/DemoSets.h"
#include "content/GltfDependencies.h"
#include "miniz.h"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace cadence::content;
void expect(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
template <class F> void rejects(F f, const char *m) {
  bool rejected = false;
  try {
    f();
  } catch (...) {
    rejected = true;
  }
  expect(rejected, m);
}
void bytes(const fs::path &p, const std::string &s) {
  fs::create_directories(p.parent_path());
  std::ofstream o(p, std::ios::binary);
  o << s;
}
std::string glbMetadata(std::string json){
  while(json.size()%4)json+=' ';
  std::string out;
  const auto word=[&](std::uint32_t v){for(int i=0;i<4;++i)out+=static_cast<char>((v>>(8*i))&255);};
  word(0x46546c67);word(2);word(static_cast<std::uint32_t>(20+json.size()));
  word(static_cast<std::uint32_t>(json.size()));word(0x4e4f534a);out+=json;return out;
}
void zip(const fs::path &p,
         const std::vector<std::pair<std::string, std::string>> &entries) {
  mz_zip_archive z{};
  expect(mz_zip_writer_init_file(&z, p.string().c_str(), 0), "zip init");
  for (const auto &[name, value] : entries)
    expect(
        mz_zip_writer_add_mem(&z, name.c_str(), value.data(), value.size(), 9),
        "zip entry");
  expect(mz_zip_writer_finalize_archive(&z), "zip finalize");
  mz_zip_writer_end(&z);
}
int main() {
  const auto root =
      fs::temp_directory_path() /
      ("cadence-set-tests-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    fs::create_directories(root);
    {
      take::Take relocation;
      relocation.actor.baseModel="old-machine/body.cast";
      relocation.actor.rigModels={"old-machine/hands.cast"};
      relocation.worldActor.baseModel="old-machine/world.cast";
      std::vector<std::string> checked;
      const auto missing=missingModels(relocation,[&](const std::string& path){checked.push_back(path);return path!="old-machine/world.cast";});
      expect(checked.size()==3&&missing==std::vector<std::string>{"old-machine/world.cast"},"model preflight bypassed supplied relocation resolver");
      expect(missingModels(relocation,[](const std::string&){return true;}).empty(),"relocated models rejected");
    }
    {
      limits::Inventory inventory;
      inventory.count = limits::entries - 1;
      inventory.add(0);
      rejects([&] { inventory.add(0); }, "entry limit exceeded");
      inventory = {};
      inventory.add(limits::fileBytes);
      rejects([&] { inventory.add(limits::fileBytes + 1); }, "file size limit exceeded");
      inventory = {};
      inventory.bytes = limits::archiveBytes - 1;
      inventory.add(1);
      rejects([&] { inventory.add(1); }, "archive size limit exceeded");
      rejects([&] { inventory.add(UINT64_MAX); }, "size overflow accepted");
      limits::manifest(limits::manifestBytes, limits::aliases);
      rejects([&] { limits::manifest(limits::manifestBytes + 1, 0); }, "manifest byte limit exceeded");
      rejects([&] { limits::manifest(0, limits::aliases + 1); }, "manifest alias limit exceeded");
      Progress cancelledProgress;
      cancelledProgress.cancel = true;
      bool cancelledFirst = false;
      try { importPack(root / "precancel", root / "does-not-exist.zip", cancelledProgress); }
      catch (const std::exception &e) { cancelledFirst = std::string(e.what()) == "Cancelled"; }
      expect(cancelledFirst && !fs::exists(root / "precancel"), "pre-cancel did IO before cancellation");
      expect(safeRelative(std::string(limits::pathBytes, 'x')) &&
             limits::zipFilenameLength(limits::pathBytes + 1), "exact path limit rejected");
      expect(!safeRelative(std::string(limits::pathBytes + 1, 'x')) &&
             !limits::zipFilenameLength(limits::pathBytes + 2), "overlong path accepted");
      const auto source = root / "limit-source";
      bytes(source, "x");
      for (const auto *destination : {"../unsafe", "assets.txt", "CADENCE-PACK.TXT"}) {
        Plan invalid;
        invalid.files.push_back({source, destination, 1, fs::last_write_time(source)});
        Progress progress;
        rejects([&] { packSet(invalid, root / "invalid-plan.zip", true, progress); }, "invalid plan destination accepted");
        expect(!fs::exists(root / "invalid-plan.zip"), "invalid plan published archive");
      }
      Plan duplicate;
      duplicate.files = {{source, "assets/test", 1, fs::last_write_time(source)},
                         {source, "assets/TEST", 1, fs::last_write_time(source)}};
      Progress progress;
      rejects([&] { packSet(duplicate, root / "duplicate-plan.zip", true, progress); }, "duplicate plan destination accepted");
      expect(!fs::exists(root / "duplicate-plan.zip"), "duplicate plan published archive");
    }
    for (const auto *s :
         {"../outside", "/absolute", "C:/drive", "nul.txt", "foo:ads",
          "foo/../bar", "foo\\bar", "foo./bar", "com1.png"})
      expect(!safeRelative(s), "unsafe path accepted");
    expect(safeRelative("assets/exported_files/bo2/models/rifle.cast"),
           "safe name rejected");
    expect(!validName("a/b"), "nested set name accepted");
    auto set = createSet(root, "Session one");
    rejects([&] { createSet(root, "Session one"); }, "set overwrite allowed");
    const auto model =
        root / "source" / "exported_files" / "bo2" / "models" / "hands.cast";
    fs::create_directories(model.parent_path());
    {
      std::ofstream out(model, std::ios::binary);
      const uint32_t header[]{cast::Document::kMagic, 1, 0, 0};
      out.write(reinterpret_cast<const char *>(header), sizeof(header));
    }
    auto map = root / "source" / "map" / "world.glb";
    bytes(map, glbMetadata(R"({"asset":{"version":"2.0"}})"));
    bytes(map.parent_path() / "textures" / "ground.png", "texture fixture");
    bytes(map.parent_path() / "collision.json", "{}");
    take::Take t;
    t.boneCount = 1;
    t.actor.baseModel = utf8(model);
    take::Sample s;
    s.pose = {scene::Mat4::identity()};
    t.samples.push_back(s);
    {
      const auto folder=root/"uri-fixture";
      const auto source=folder/"triangle.gltf";
      const std::string json=R"({"asset":{"version":"2.0"},"buffers":[{"uri":"geometry.dat","byteLength":36},{"uri":"extensionless","byteLength":1},{"uri":"geometry%20data.dat","byteLength":1},{"uri":"literal%25.dat","byteLength":1},{"uri":"data:application/octet-stream;base64,AA==","byteLength":1}],"images":[{"uri":"image%20data.custom"}]})";
      bytes(source,json);
      for(const auto name:{"geometry.dat","extensionless","geometry data.dat","literal%.dat","image data.custom"})bytes(folder/name,std::string(36,'\0'));
      auto dependencies=gltf::metadataDependencies(source);
      expect(dependencies.size()==5,"URI dependencies or embedded data handling failed");
      bytes(folder/"metadata.glb",glbMetadata(json));
      expect(gltf::metadataDependencies(folder/"metadata.glb")==dependencies,"GLB metadata dependencies differ");
      bytes(folder/"escaped.gltf",R"({"buffers":[{"uri":"geometry\u0020data.dat"},{"uri":"sub\/file.dat"}]})");
      const auto escaped=gltf::metadataDependencies(folder/"escaped.gltf");
      expect(escaped.size()==2&&escaped[0].filename()=="geometry data.dat"&&escaped[1].parent_path().filename()=="sub","JSON URI decoding differs");
      expect(gltf::decodedResourcePath(folder,"missing/nested/resource.dat")->filename()=="resource.dat","missing nested resource rejected");
      bytes(folder/"escaped.gltf",R"({"buffers":[{"uri":"bad\u0000.dat"}]})");
      rejects([&]{gltf::metadataDependencies(folder/"escaped.gltf");},"JSON NUL URI admitted");
      fs::remove(folder/"escaped.gltf");
      for(const auto uri:{"../escape.dat","%2e%2e%2fescape.dat","%2e%2e%5cescape.dat","%00.dat","C%3a/file","%2froot","bad%","bad%xx"})
        rejects([&]{gltf::decodedResourcePath(folder,uri);},"unsafe URI admitted");
      expect(gltf::decodedResourcePath(folder,"%252e%252e/file")->parent_path().filename()=="%2e%2e","URI decoded twice");
      std::error_code linkError;fs::create_directory_symlink(root,folder/"escape-link",linkError);
      if(!linkError)rejects([&]{gltf::decodedResourcePath(folder,"escape-link/outside");},"URI symlink escape admitted");
      std::error_code ownerLinkError;const auto alternateRoot=root/"alternate-map-root";
      fs::create_directory_symlink(folder,alternateRoot,ownerLinkError);
      if(!ownerLinkError){
        const auto alternate=gltf::decodedResourcePath(alternateRoot,"geometry.dat");
        expect(alternate&&*alternate==(alternateRoot/"geometry.dat").lexically_normal(),"dependency lost owning map root spelling");
        expect(fs::equivalent(*alternate,folder/"geometry.dat"),"alternate dependency changed physical source");
        fs::remove(alternateRoot);
      }
      expect(*gltf::decodedResourcePath(folder/".","geometry.dat")== (folder/"geometry.dat").lexically_normal(),"dependency normalization changed owning root");
      bytes(folder/"truncated.glb","glTF");
      rejects([&]{gltf::metadataDependencies(folder/"truncated.glb");},"truncated GLB admitted");
      fs::remove(folder/"truncated.glb");
      if(!linkError)fs::remove(folder/"escape-link");
      Metadata uriMetadata;uriMetadata.map=utf8(source);
      const auto uriSet=root/"uri-set";fs::create_directories(uriSet);saveDemo(t,uriMetadata,uriSet,"uri");
      Progress uriProgress;auto uriPlan=planSet(uriSet,uriProgress);
      expect(uriPlan.missing.empty(),"existing nonstandard URI dependency missing");
      for(const auto& dependency:dependencies)expect(std::any_of(uriPlan.files.begin(),uriPlan.files.end(),[&](const auto& file){return fs::equivalent(file.source,dependency);}),"URI dependency omitted from plan");
      fs::remove(folder/"extensionless");
      uriPlan=planSet(uriSet,uriProgress);
      expect(uriPlan.missing.size()==1&&uriPlan.missing.front().find("extensionless")!=std::string::npos,"missing URI not reported");
      auto nestedJson=scene::codm::parseJson(json);
      nestedJson["buffers"].push_back({{"uri","missing/nested/resource.dat"},{"byteLength",1}});
      bytes(source,nestedJson.dump());
      uriPlan=planSet(uriSet,uriProgress);
      expect(uriPlan.missing.size()==2&&std::any_of(uriPlan.missing.begin(),uriPlan.missing.end(),[](const auto& name){return name.find("resource.dat")!=std::string::npos;}),"nested missing URI not reported");
    }
    Metadata m;
    m.map = utf8(map);
    m.mapScale = 2.54f;
    m.construction = {utf8(model)};
    saveDemo(t, m, set, "one");
    expect(listDemos(set).size() == 1, "save missing");
    expect(readMetadata(set / "one.c_dm").mapScale == 2.54f, "scale lost");
    {
      std::ofstream out(metadataPath(set / "one.c_dm"), std::ios::app);
      out << "\"truncated construction reference";
    }
    rejects([&] { readMetadata(set / "one.c_dm"); },
            "truncated quoted metadata silently discarded");
    writeMetadata(set / "one.c_dm", m);
    rejects([&] { saveDemo(t, m, set, "one"); }, "demo overwrite allowed");
    Progress p;
    auto plan = planSet(set, p);
    saveDemo(t, m, set, "added-after-review");
    rejects([&] { packSet(plan, root / "stale.zip", false, p); },
            "changed demo inventory accepted");
    fs::remove(set / "added-after-review.c_dm");
    fs::remove(metadataPath(set / "added-after-review.c_dm"));
    expect(plan.missing.empty(), "fixture dependencies missing");
    expect(plan.files.size() == 6, "dependency dedup or map companion failure");
    auto archive = root / "set.zip";
    packSet(plan, archive, true, p);
    rejects([&] { packSet(plan, archive, true, p); },
            "archive overwrite allowed");
    auto other = root / "recipient";
    auto id = importPack(other, archive, p);
    expect(listSets(other).size() == 1, "imported set missing");
    auto roots = activatePacks(other);
    expect(roots.size() == 1, "pack catalog root missing");
    preferPack(id);
    fs::rename(root / "source", root / "hidden-source");
    expect(fs::is_regular_file(resolve(model)), "absolute model not relocated");
    expect(fs::is_regular_file(resolve(map)), "map not relocated");
    expect(missingModels(t).empty(), "recipient dependencies fail");
    auto imported = listDemos(listSets(other).front()).front();
    take::Take loaded;
    std::string error;
    expect(take::load(imported, loaded, error), "packed replay fails load");
    expect(loaded.actor.baseModel == t.actor.baseModel,
           "pack rewrote demo content");
    expect(readMetadata(imported).pack == id, "set pack identity missing");
    auto installedModel = resolve(model);
    auto all = packs(other);
    setEnabled(all.front(), false);
    activatePacks(other);
    expect(!packActive(id), "disabled pack active");
    expect(!fs::exists(resolve(model)), "disabled pack resolves");
    expect(resolve(installedModel).empty(),
           "disabled physical path bypasses pack state");
    setEnabled(all.front(), true);
    activatePacks(other);
    auto repack = planSet(imported.parent_path(), p);
    expect(repack.missing.empty(), "repack missing dependencies");
    packSet(repack, root / "repack.zip", true, p);
    auto third = root / "third";
    auto thirdId = importPack(third, root / "repack.zip", p);
    activatePacks(third);
    preferPack(thirdId);
    expect(fs::exists(resolve(map)), "repack lost original map alias");
    expect(fs::exists(resolve(model)), "repack lost original model alias");
    auto missingPlan = plan;
    missingPlan.missing.push_back("missing.cast");
    rejects([&] { packSet(missingPlan, root / "bad.zip", true, p); },
            "incomplete assets allowed");
    packSet(missingPlan, root / "listing.zip", false, p);
    auto listingId =
        importPack(root / "listing-install", root / "listing.zip", p);
    expect(readMetadata(
               listDemos(listSets(root / "listing-install").front()).front())
               .pack.empty(),
           "assets-only-list requires nonexistent pack");
    p.cancel = true;
    rejects([&] { packSet(repack, root / "cancel.zip", true, p); },
            "cancel ignored");
    expect(!fs::exists(root / "cancel.zip"), "cancel published zip");
    rejects([&] { importPack(other, archive, p); }, "cancel import ignored");
    p.cancel = false;
    zip(root / "traversal.zip", {{"../escape.txt", "bad"}});
    rejects([&] { importPack(other, root / "traversal.zip", p); },
            "zip traversal accepted");
    expect(!fs::exists(other / "escape.txt"), "zip escaped root");
    zip(root / "duplicate.zip", {{"a.txt", "1"}, {"A.txt", "2"}});
    rejects([&] { importPack(other, root / "duplicate.zip", p); },
            "case duplicate accepted");
#ifdef _WIN32
    const std::string upper = "\xC3\x84.cast", lower = "\xC3\xA4.cast";
    const auto probe = root / "unicode-case-probe";
    bytes(probe / fs::u8path(upper), "probe");
    if (fs::exists(probe / fs::u8path(lower))) {
      zip(root / "unicode-duplicate.zip", {{"css/models/" + upper, "first"},
                                           {"css/models/" + lower, "second"}});
      const auto before = packs(other).size();
      rejects([&] { importPack(other, root / "unicode-duplicate.zip", p); },
              "filesystem-equivalent Unicode paths accepted");
      expect(packs(other).size() == before, "Unicode collision published pack");
      for (const auto &entry : fs::directory_iterator(other / "packs"))
        expect(!utf8(entry.path().filename()).starts_with(".install-"),
               "Unicode collision left staging files");
    }
#endif
    for (const auto prefix : {"exported_files/", "_files/"}) {
      zip(root / "logical-conflict.zip",
          {{"css/models/test.cast", "first asset"},
           {std::string(prefix) + "css/models/test.cast", "second asset"}});
      const auto previousPacks = packs(other).size();
      rejects([&] { importPack(other, root / "logical-conflict.zip", p); },
              "normalized asset collision accepted");
      expect(packs(other).size() == previousPacks,
             "conflicting raw ZIP published a pack");
      for (const auto &entry : fs::directory_iterator(other / "packs"))
        expect(!utf8(entry.path().filename()).starts_with(".install-"),
               "conflicting raw ZIP left staging files");
    }
    zip(root / "user.zip", {{"css/models/test.cast", "fixture"},
                            {"css/textures/test.png", "pixels"}});
    importPack(other, root / "user.zip", p);
    activatePacks(other);
    expect(fs::is_regular_file(resolve(
               fs::path("Z:/unavailable/exported_files/css/models/test.cast"))),
           "user ZIP logical mapping failed");
    const auto nestedHome=root/"exported_files/tools/Cadence";
    importPack(nestedHome,root/"user.zip",p);
    activatePacks(nestedHome);
    const auto nestedAsset=resolve(fs::path("Z:/unavailable/exported_files/css/models/test.cast"));
    expect(fs::is_regular_file(nestedAsset)&&exportKey(nestedAsset)=="css/models/test.cast",
           "recipient ancestor export folder hijacked raw ZIP identity");
    zip(root/"ambiguous-roots.zip",{{"exported_files/tools/exported_files/css/models/test.cast","fixture"}});
    rejects([&]{importPack(nestedHome,root/"ambiguous-roots.zip",p);},
            "ambiguous nested export roots guessed a game");
    activatePacks(other);
    const auto deep = "assets/exported_files/css/models/" +
                      std::string(190, 'x') + "/test.cast";
    zip(root / "deep.zip", {{deep, "fixture"}});
    importPack(other, root / "deep.zip", p);
    activatePacks(other);
    expect(fs::is_regular_file(
               resolve(fs::path("Z:/old/exported_files/css/models") /
                       std::string(190, 'x') / "test.cast")),
           "long Windows path failed");
    zip(root / "partial.zip",
        {{"cadence-pack.txt",
          "CADENCEPACK 1\n\"a\" \"assets/a.cast\"\n\"b\" \"assets/b.cast\"\n"},
         {"assets/a.cast", "fixture"}});
    rejects([&] { importPack(other, root / "partial.zip", p); },
            "incomplete managed pack accepted");
    zip(root / "truncated-alias.zip",
        {{"cadence-pack.txt", "CADENCEPACK 2 assets\n\"a\" \"assets/a.cast\"\n\"unfinished alias"},
         {"assets/a.cast", "fixture"}});
    rejects([&] { importPack(other, root / "truncated-alias.zip", p); },
            "truncated quoted pack alias silently discarded");
    zip(root / "empty-assets.zip",
        {{"cadence-pack.txt",
          "CADENCEPACK 2 assets\n\"a\" \"assets/a.cast\"\n"}});
    rejects([&] { importPack(other, root / "empty-assets.zip", p); },
            "fully stripped asset bundle accepted as listing");
    auto brokenHome = root / "broken-install";
    auto brokenId = importPack(brokenHome, archive, p);
    activatePacks(brokenHome);
    fs::remove(resolve(model));
    expect(!packs(brokenHome).front().error.empty(),
           "damaged installed pack not reported");
    activatePacks(brokenHome);
    expect(!packActive(brokenId), "damaged installed pack activated");
    const fs::path first =
        root / "packs/a/assets/exported_files/bo2/models/test.cast";
    const fs::path second =
        root / "packs/b/assets/exported_files/bo2/models/test.cast";
    const fs::path original = "Z:/source/exported_files/bo2/models/test.cast";
    mountPaths(
        {{"a", {{"game:bo2/models/test.cast", first}}, root / "packs/a"},
         {"b", {{"game:bo2/models/test.cast", second}}, root / "packs/b"}});
    preferPack("b");
    expect(resolve(original) == second, "preferred pack ignored");
    expect(resolve(first) == first, "preferred pack hijacked mounted asset");
    expect(resolveFor(original, first) == first, "owning pack ignored");
    {
      ResolutionScope scope("");
      expect(resolve(original) == first, "empty scope inherited gameplay pack");
      {
        ResolutionScope nested("b");
        expect(resolve(original) == second, "nested pack ignored");
      }
      expect(resolve(original) == first, "nested scope not restored");
    }
    expect(resolve(original) == second, "global preference not restored");
    // Distinct installed paths can share an old absolute alias without sharing
    // an exported_files destination. Packing must not silently pick the last.
    auto conflictingSet = createSet(root, "Conflicting versions");
    const auto assetA = root / "alias-a" / "assets" / "hands.cast";
    const auto assetB = root / "alias-b" / "assets" / "hands.cast";
    bytes(assetA, "first");
    bytes(assetB, "second");
    const std::string oldAlias = "Z:/legacy/hands.cast";
    mountPaths({{"alias-a", {{key(fs::u8path(oldAlias)), assetA}},
                 root / "alias-a"},
                {"alias-b", {{key(fs::u8path(oldAlias)), assetB}},
                 root / "alias-b"}});
    auto conflictTake = t;
    conflictTake.actor.baseModel = oldAlias;
    Metadata conflictMetadata;
    conflictMetadata.pack = "alias-a";
    saveDemo(conflictTake, conflictMetadata, conflictingSet, "first");
    conflictMetadata.pack = "alias-b";
    saveDemo(conflictTake, conflictMetadata, conflictingSet, "second");
    bool aliasRejected = false;
    try {
      planSet(conflictingSet, p);
    } catch (const std::exception &e) {
      aliasRejected = std::string(e.what()).find("Conflicting versions of") !=
                      std::string::npos;
    }
    expect(aliasRejected, "conflicting replay aliases silently overwritten");
    const auto overlapHome = root / "overlap-home";
    for (const auto *name : {"c", "a", "b"}) {
      const auto folder = overlapHome / "packs" / name;
      bytes(folder / "assets/exported_files/bo2/models/shared.cast", name);
      bytes(folder / "cadence-pack.txt",
            "CADENCEPACK 2 assets\n\"game:bo2/models/shared.cast\" \"assets/exported_files/bo2/models/shared.cast\"\n");
    }
    auto overlaps = packs(overlapHome);
    expect(overlaps.size() == 3, "overlap fixture packs missing");
    for (const auto &pack : overlaps) {
      expect(pack.conflicts == 1 && pack.overlaps.size() == 1,
             "overlaps should count unique paths for every participant");
      expect(pack.overlaps.front().defaultPack == "a" &&
                 pack.overlaps.front().packs == std::vector<std::string>{"a", "b", "c"},
             "overlap priority/details disagree with resolver order");
    }
    const fs::path sharedOriginal = "Z:/source/exported_files/bo2/models/shared.cast";
    const auto sharedPath = [&](const char *name) {
      return longPath(overlapHome / "packs" / name /
                      "assets/exported_files/bo2/models/shared.cast");
    };
    activatePacks(overlapHome);
    {
      ResolutionScope unpreferred("");
      expect(resolve(sharedOriginal) == sharedPath("a"),
             "reported fallback disagrees with active resolver");
      ResolutionScope preferred("c");
      expect(resolve(sharedOriginal) == sharedPath("c") &&
                 resolveFor(sharedOriginal, sharedPath("c")) == sharedPath("c"),
             "default fallback overrode replay/owning pack");
    }
    setEnabled(overlaps.front(), false);
    overlaps = packs(overlapHome);
    expect(overlaps.front().conflicts == 0 &&
               overlaps[1].overlaps.front().defaultPack == "b",
           "disabled pack still owns overlap priority");
    activatePacks(overlapHome);
    {
      ResolutionScope unpreferred("");
      expect(resolve(sharedOriginal) == sharedPath("b"),
             "disabled pack fallback disagrees with resolver");
      ResolutionScope preferred("c");
      expect(resolve(sharedOriginal) == sharedPath("c"),
             "disabled default changed preferred-pack priority");
    }
    bytes(overlapHome / "packs/b/assets/exported_files/bo2/models/shared.cast", "b");
    bytes(overlapHome / "packs/b/cadence-pack.txt", "invalid manifest");
    overlaps = packs(overlapHome);
    expect(overlaps[1].conflicts == 0 && overlaps[2].conflicts == 0,
           "invalid pack participates in overlaps");
    mountPaths({});
    preferPack({});
    fs::remove_all(longPath(root));
    std::cout << "Demo sets: save, metadata, ZIP64 roundtrip, recipient "
                 "relocation, pack toggles, raw ZIP import, traversal, "
                 "duplicates, missing dependencies, cancellation passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\nFixture retained: " << root << '\n';
    return 1;
  }
}
