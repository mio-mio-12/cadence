#include "content/DemoSets.h"
#include "content/GltfDependencies.h"
#include "miniz.h"
#include "scene/CastScene.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace cadence::content {
namespace {
void check(bool ok, const std::string &message) {
  if (!ok)
    throw std::runtime_error(message);
}
void cancelled(Progress &p) { check(!p.cancel.load(), "Cancelled"); }
std::string token() {
  return std::to_string(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
}
fs::path unique(const fs::path &parent, const std::string &name) {
  auto p = parent / fs::u8path(name);
  for (unsigned n = 2; fs::exists(p); ++n)
    p = parent / fs::u8path(name + " (" + std::to_string(n) + ")");
  return p;
}
void writeText(const fs::path &p, const std::string &value) {
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  out << value;
  out.close();
  check(bool(out), "Cannot write " + utf8(p));
}
struct Cleanup {
  fs::path path;
  bool committed{};
  ~Cleanup() {
    if (!committed) {
      std::error_code ec;
      fs::remove_all(path, ec);
    }
  }
};
using File = std::unique_ptr<FILE, decltype(&fclose)>;
File openFile(const fs::path &p, bool write) {
#ifdef _WIN32
  auto f = _wfopen(p.c_str(), write ? L"wb" : L"rb");
#else
  auto f = fopen(p.c_str(), write ? "wb" : "rb");
#endif
  check(f != nullptr, "Cannot open " + utf8(p));
  return {f, &fclose};
}
struct Zip {
  mz_zip_archive z{};
  bool writer{};
  ~Zip() {
    if (z.m_pState) {
      if (writer)
        mz_zip_writer_end(&z);
      else
        mz_zip_reader_end(&z);
    }
  }
};
std::vector<std::string> models(const take::Take &t) {
  std::set<std::string> result;
  const auto add = [&](const take::ActorManifest &a) {
    if (!a.baseModel.empty())
      result.insert(a.baseModel);
    for (const auto &p : a.rigModels)
      result.insert(p);
    for (const auto &p : a.attachedModels)
      result.insert(p.path);
  };
  add(t.actor);
  add(t.worldActor);
  add(t.botActor);
  for (const auto &a : t.actorSlots)
    add(a);
  for (const auto &a : t.worldActorSlots)
    add(a);
  return {result.begin(), result.end()};
}
std::string manifest(const Plan &p, bool assets = true, Progress *progress = nullptr) {
  limits::manifest(0, p.aliases.size());
  std::ostringstream out;
  out << "CADENCEPACK 2 " << (assets ? "assets" : "listing") << "\n";
  for (const auto &[alias, path] : p.aliases) {
    if (progress) cancelled(*progress);
    check(alias.size() <= 32768 && safeRelative(path) && path.starts_with("assets/"), "Unsafe pack reference");
    out << std::quoted(alias) << ' ' << std::quoted(path) << '\n';
    limits::manifest(static_cast<uint64_t>(out.tellp()), p.aliases.size());
  }
  return out.str();
}
std::map<std::string, std::string> readManifest(const fs::path &p,
                                                int *assetMode = nullptr, Progress *progress = nullptr) {
  if (progress) cancelled(*progress);
  limits::manifest(fs::file_size(p), 0);
  std::ifstream in(p);
  std::string header;
  int version{};
  check(bool(in >> header >> version) && header == "CADENCEPACK" &&
            (version == 1 || version == 2),
        "Unsupported pack manifest");
  int mode = -1; // v1 did not distinguish a listing from a complete bundle.
  if (version == 2) {
    std::string kind;
    check(bool(in >> kind) && (kind == "assets" || kind == "listing"),
          "Invalid pack type");
    mode = kind == "assets" ? 1 : 0;
  }
  if (assetMode)
    *assetMode = mode;
  std::map<std::string, std::string> result;
  std::string a, b;
  while (in >> std::ws && in.peek() != std::char_traits<char>::eof()) {
    if (progress) cancelled(*progress);
    check(bool(in >> std::quoted(a)), "Incomplete pack manifest alias");
    check(bool(in >> std::quoted(b)), "Incomplete pack manifest");
    check(a.size() <= 32768 && safeRelative(b) && b.starts_with("assets/"),
          "Unsafe pack reference");
    check(result.emplace(a, b).second, "Duplicate pack reference");
    limits::manifest(0, result.size());
  }
  check(in.eof(), "Invalid pack manifest");
  return result;
}
} // namespace
bool safeRelative(std::string name) {
  if (name.empty() || name.size() > limits::pathBytes || name.front() == '/' ||
      name.front() == '\\' || name.find('\\') != std::string::npos ||
      name.find(':') != std::string::npos)
    return false;
  if (name.back() == '/')
    name.pop_back();
  std::istringstream in(name);
  std::string part;
  while (std::getline(in, part, '/')) {
    if (part.empty() || part == "." || part == ".." || part.back() == '.' ||
        part.back() == ' ')
      return false;
    for (unsigned char c : part)
      if (c < 32 || c == '<' || c == '>' || c == '"' || c == '|' || c == '?' ||
          c == '*')
        return false;
    auto stem = key(fs::u8path(part.substr(0, part.find('.'))));
    if (stem == "con" || stem == "conin$" || stem == "conout$" ||
        stem == "prn" || stem == "aux" || stem == "nul" ||
        (stem.size() == 4 &&
         (stem.starts_with("com") || stem.starts_with("lpt")) &&
         stem[3] >= '0' && stem[3] <= '9'))
      return false;
  }
  return !name.empty();
}
bool validName(const std::string &name) {
  return !name.empty() && name.front() != '.' && name.size() <= 120 &&
         safeRelative(name) && name.find('/') == std::string::npos;
}
fs::path metadataPath(const fs::path &demo) {
  auto p = demo;
  p += ".setinfo";
  return p;
}
Metadata readMetadata(const fs::path &demo) {
  Metadata m;
  std::ifstream in(metadataPath(demo));
  if (!in)
    return m;
  check(fs::file_size(metadataPath(demo)) <= 16 * 1024 * 1024,
        "Demo metadata too large");
  std::string header;
  int version{};
  check(bool(in >> header >> version) && header == "CADENCEDEMO" &&
            version == 1,
        "Unsupported demo metadata");
  check(bool(in >> std::quoted(m.map) >> m.mapScale >> std::quoted(m.pack)) &&
            std::isfinite(m.mapScale) && m.mapScale > 0,
        "Invalid map metadata");
  std::string path;
  while (in >> std::ws && in.peek() != std::char_traits<char>::eof()) {
    check(bool(in >> std::quoted(path)), "Incomplete construction reference");
    check(m.construction.size() < 100000, "Too many construction references");
    m.construction.push_back(path);
  }
  check(in.eof(), "Invalid demo metadata");
  return m;
}
void writeMetadata(const fs::path &demo, const Metadata &m) {
  std::ostringstream out;
  out << std::setprecision(9) << "CADENCEDEMO 1\n"
      << std::quoted(m.map) << ' ' << m.mapScale << ' ' << std::quoted(m.pack)
      << '\n';
  for (const auto &p : m.construction)
    out << std::quoted(p) << '\n';
  auto target = metadataPath(demo), temp = target;
  temp += ".partial-" + token();
  Cleanup cleanup{temp};
  writeText(temp, out.str());
#ifdef _WIN32
  check(MoveFileExW(temp.c_str(), target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
        "Cannot publish demo metadata");
#else
  fs::rename(temp, target);
#endif
  cleanup.committed = true;
}
std::vector<fs::path> listSets(const fs::path &home) {
  std::vector<fs::path> out;
  const auto root = longPath(home / "demos");
  if (fs::is_directory(root))
    for (const auto &e : fs::directory_iterator(root))
      if (e.is_directory() && !e.path().filename().string().starts_with('.'))
        out.push_back(e.path());
  std::sort(out.begin(), out.end());
  return out;
}
std::vector<fs::path> listDemos(const fs::path &set) {
  std::vector<fs::path> out;
  if (!set.empty() && fs::is_directory(longPath(set)))
    for (const auto &e : fs::directory_iterator(longPath(set)))
      if (e.is_regular_file() && key(e.path().extension()) == ".c_dm")
        out.push_back(e.path());
  std::sort(out.begin(), out.end());
  return out;
}
fs::path createSet(const fs::path &home, const std::string &name) {
  check(validName(name), "Invalid set name");
  auto p = longPath(home / "demos" / fs::u8path(name));
  check(!fs::exists(p), "A set with that name already exists");
  fs::create_directories(p);
  return p;
}
void saveDemo(const take::Take &t, const Metadata &m, const fs::path &set,
              const std::string &name) {
  check(validName(name), "Invalid demo name");
  check(!t.samples.empty(), "No recording to save");
  auto target = set / fs::u8path(name);
  if (key(target.extension()) != ".c_dm")
    target += ".c_dm";
  check(!fs::exists(target) && !fs::exists(metadataPath(target)),
        "A demo with that name already exists");
  fs::create_directories(set);
  auto temp = target;
  temp += ".partial-" + token();
  Cleanup cleanup{temp};
  std::string error;
  check(take::save(t, temp, error), error);
  writeMetadata(target, m);
  try {
    fs::rename(temp, target);
  } catch (...) {
    std::error_code ec;
    fs::remove(metadataPath(target), ec);
    throw;
  }
  cleanup.committed = true;
}
void importDemo(const fs::path &source, const fs::path &set) {
  take::Take t;
  std::string error;
  check(take::load(source, t, error), error);
  check(validName(utf8(source.filename())), "Invalid demo name");
  const auto target = set / source.filename();
  check(!fs::exists(target) && !fs::exists(metadataPath(target)),
        "A demo with that name already exists");
  const auto metadata = readMetadata(source);
  fs::create_directories(set);
  auto temp = target;
  temp += ".partial-" + token();
  Cleanup cleanup{temp};
  fs::copy_file(source, temp);
  writeMetadata(target, metadata);
  try {
    fs::rename(temp, target);
  } catch (...) {
    std::error_code ec;
    fs::remove(metadataPath(target), ec);
    throw;
  }
  cleanup.committed = true;
}
std::vector<std::string> missingModels(const take::Take &t,
    const std::function<bool(const std::string&)>& available) {
  std::vector<std::string> missing;
  for (const auto &p : models(t))
    if (!(available ? available(p) : fs::is_regular_file(resolve(fs::u8path(p)))))
      missing.push_back(p);
  return missing;
}
Plan planSet(const fs::path &set, Progress &progress) {
  ObservationPause pause;
  Plan plan;
  plan.setName = utf8(set.filename());
  std::map<std::string, std::string> sourceDest;
  std::map<std::string, std::string> destSource;
  std::vector<fs::path> casts;
  std::set<std::string> processed;
  const auto addAlias = [&](const std::string &alias, const std::string &dest) {
    const auto [it, inserted] = plan.aliases.emplace(alias, dest);
    check(inserted || it->second == dest,
          "Conflicting versions of " + alias +
              "; separate these demos into different sets");
  };
  const auto add = [&](const fs::path &original,
                       const std::string &preferred =
                           std::string{}) -> std::string {
    cancelled(progress);
    if (original.empty())
      return {};
    auto path = resolve(original);
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
      plan.missing.push_back(utf8(original));
      return {};
    }
    check(!fs::is_symlink(fs::symlink_status(path)),
          "Linked asset cannot be packed: " + utf8(path));
    const auto k = key(path);
    std::string dest;
    if (auto i = sourceDest.find(k); i != sourceDest.end())
      dest = i->second;
    else {
      dest = preferred;
      auto logical = exportKey(path);
      if (dest.empty()) {
        auto absolute = fs::absolute(fs::u8path(utf8(path)));
        auto drive = utf8(absolute.root_name());
        std::replace(drive.begin(), drive.end(), ':', '_');
        std::replace(drive.begin(), drive.end(), '\\', '_');
        std::replace(drive.begin(), drive.end(), '/', '_');
        dest = logical.empty()
                   ? "assets/files/" + (drive.empty() ? "root" : drive) + "/" +
                         utf8(absolute.relative_path())
                   : "assets/exported_files/" + logical;
      }
      check(safeRelative(dest), "Unsupported asset filename: " + dest);
      if (auto i = destSource.find(key(fs::u8path(dest)));
          i != destSource.end())
        check(i->second == k, "Conflicting versions of " + dest +
                                  "; separate these demos into different sets");
      destSource[key(fs::u8path(dest))] = k;
      sourceDest[k] = dest;
      plan.files.push_back(
          {path, dest, fs::file_size(path), fs::last_write_time(path)});
      plan.bytes += fs::file_size(path);
      if (key(path.extension()) == ".cast")
        casts.push_back(path);
    }
    if (dest.starts_with("assets/")) {
      addAlias(key(original), dest);
      addAlias(k, dest);
      const auto logical = exportKey(original), app = appKey(original);
      if (!logical.empty())
        addAlias("game:" + logical, dest);
      if (!app.empty())
        addAlias("app:" + app, dest);
      for (const auto &alias : aliasesFor(path))
        addAlias(alias, dest);
    }
    return dest;
  };
  auto demos = listDemos(set);
  plan.sourceSet = set;
  plan.reviewedDemos = demos;
  check(!demos.empty(), "This set has no demos");
  for (const auto &demo : demos) {
    cancelled(progress);
    take::Take t;
    std::string error;
    check(take::load(demo, t, error), utf8(demo) + ": " + error);
    add(demo, "demos/" + utf8(demo.filename()));
    const auto info = readMetadata(demo);
    ResolutionScope preferred(info.pack);
    if (fs::exists(metadataPath(demo)))
      add(metadataPath(demo), "demos/" + utf8(metadataPath(demo).filename()));
    for (const auto &p : models(t))
      add(fs::u8path(p));
    for (const auto &p : info.construction)
      add(fs::u8path(p));
    if (info.map.empty())
      plan.missing.push_back("Map not recorded: " + utf8(demo.filename()));
    else {
      auto map = resolve(fs::u8path(info.map));
      if (!fs::is_regular_file(map))
        plan.missing.push_back(info.map);
      else {
        auto root = map.parent_path();
        check(root != root.root_path(), "Map must be in its own folder");
        const auto dest = add(map);
        addAlias(key(fs::u8path(info.map)), dest);
        // URI dependencies are authoritative regardless of filename extension.
        // Inspect only metadata, never decode the map's potentially huge BIN.
        for(const auto& resource:gltf::metadataDependencies(map))add(resource);
        for (const auto &file : fs::recursive_directory_iterator(root)) {
          cancelled(progress);
          check(!file.is_symlink(),
                "Map folder contains a link: " + utf8(file.path()));
          if (file.is_regular_file()) {
            const auto ext = key(file.path().extension());
            static const std::set<std::string> allowed{
                ".glb",         ".gltf", ".c2m", ".cast", ".bin",
                ".png",         ".webp", ".jpg", ".jpeg", ".tga",
                ".dds",         ".iwi",  ".bmp", ".json", ".castnav",
                ".cadenceents", ".npz",  ".ktx", ".ktx2"};
            if (allowed.contains(ext))
              add(file.path());
          }
        }
      }
    }
  }
  // Resolve material fallbacks through the same code used for rendering.
  for (size_t i = 0; i < casts.size(); ++i) {
    cancelled(progress);
    progress.total = casts.size();
    progress.done = i;
    if (!processed.insert(key(casts[i])).second)
      continue;
    const auto path = casts[i];
    for (const auto extension : {".json", ".iwweapon"}) {
      auto companion = path;
      companion.replace_extension(extension);
      if (fs::is_regular_file(companion))
        add(companion);
    }
    auto doc = cast::Document::load(path);
    check(doc.valid(), "Invalid CAST: " + utf8(path));
    auto scene = scene::buildScene(doc, false);
    for (const auto &mesh : scene.meshes)
      for (const auto &p :
           {mesh.albedoPath, mesh.normalPath, mesh.specularPath,
            mesh.metalnessPath, mesh.roughnessPath, mesh.emissivePath})
        if (!p.empty())
          add(p);
  }
  std::sort(plan.missing.begin(), plan.missing.end());
  plan.missing.erase(std::unique(plan.missing.begin(), plan.missing.end()),
                     plan.missing.end());
  progress.done = progress.total.load();
  return plan;
}
void packSet(const Plan &plan, const fs::path &archive, bool assets,
             Progress &progress) {
  cancelled(progress);
  check(!fs::exists(archive), "Archive already exists; choose another name");
  const auto checkInventory = [&] {
    check(plan.sourceSet.empty() ||
              listDemos(plan.sourceSet) == plan.reviewedDemos,
          "Set contents changed; review assets again");
  };
  checkInventory();
  check(!assets || plan.missing.empty(),
        "Missing dependencies; review the set before packing with assets");
  const auto manifestText = manifest(plan, assets, &progress);
  std::ostringstream listing;
  listing << "Cadence set: " << plan.setName << "\n";
  for (const auto &e : plan.files) {
    cancelled(progress);
    if (e.destination.starts_with("assets/")) listing << e.destination << "\n";
  }
  for (const auto &p : plan.missing) {
    cancelled(progress);
    listing << "MISSING: " << p << '\n';
  }
  const auto listingText = listing.str();
  limits::Inventory inventory;
  inventory.add(manifestText.size());
  inventory.add(listingText.size());
  std::set<std::string> destinations{"assets.txt", "cadence-pack.txt"};
  for (const auto &e : plan.files)
    if (assets || !e.destination.starts_with("assets/")) {
      cancelled(progress);
      check(safeRelative(e.destination), "Unsafe ZIP path: " + e.destination);
      check(destinations.insert(key(fs::u8path(e.destination))).second,
            "Duplicate ZIP path: " + e.destination);
      inventory.add(e.size);
      check(fs::is_regular_file(e.source) &&
                fs::file_size(e.source) == e.size &&
                fs::last_write_time(e.source) == e.modified,
            "Files changed; review assets again");
    }
  check(fs::space(archive.parent_path()).available > inventory.bytes + 1024 * 1024,
        "Not enough free disk space");
  auto temp = archive;
  temp += ".partial-" + token();
  Cleanup cleanup{temp};
  auto file = openFile(temp, true);
  Zip zip;
  zip.writer = true;
  check(mz_zip_writer_init_cfile(&zip.z, file.get(), MZ_ZIP_FLAG_WRITE_ZIP64),
        "Cannot create ZIP");
  const auto text = [&](const char *name, const std::string &value) {
    check(mz_zip_writer_add_mem(&zip.z, name, value.data(), value.size(),
                                MZ_BEST_COMPRESSION),
          "Cannot write ZIP metadata");
  };
  text("assets.txt", listingText);
  text("cadence-pack.txt", manifestText);
  progress.done = 0;
  progress.total = plan.files.size();
  for (const auto &e : plan.files) {
    cancelled(progress);
    if (assets || !e.destination.starts_with("assets/")) {
      std::ifstream input(e.source, std::ios::binary);
      struct Reader {
        std::ifstream *input;
        Progress *progress;
      };
      Reader reader{&input, &progress};
      check(
          mz_zip_writer_add_read_buf_callback(
              &zip.z, e.destination.c_str(),
              [](void *ptr, mz_uint64 offset, void *data, size_t n) -> size_t {
                auto &r = *static_cast<Reader *>(ptr);
                if (r.progress->cancel)
                  return 0;
                r.input->seekg(offset);
                r.input->read(static_cast<char *>(data), n);
                return static_cast<size_t>(r.input->gcount());
              },
              &reader, e.size, nullptr, nullptr, 0, MZ_BEST_COMPRESSION,
              nullptr, 0, nullptr, 0),
          progress.cancel ? "Cancelled" : "Failed packing " + utf8(e.source));
      check(fs::file_size(e.source) == e.size &&
                fs::last_write_time(e.source) == e.modified,
            "Files changed while packing; review assets again");
    }
    ++progress.done;
  }
  cancelled(progress);
  checkInventory();
  check(mz_zip_writer_finalize_archive(&zip.z), "Cannot finish ZIP");
  check(mz_zip_writer_end(&zip.z), "Cannot close ZIP");
  file.reset();
  cancelled(progress);
  fs::rename(temp, archive);
  cleanup.committed = true;
}
std::string importPack(const fs::path &home, const fs::path &archive,
                       Progress &progress) {
  cancelled(progress);
  fs::create_directories(longPath(home / "packs"));
  auto stage = longPath(home / "packs" / (".install-" + token()));
  check(fs::create_directory(stage), "Cannot create staging folder");
  Cleanup cleanup{stage};
  auto input = openFile(archive, false);
  Zip zip;
  check(mz_zip_reader_init_cfile(&zip.z, input.get(), 0, 0), "Invalid ZIP");
  const auto count = mz_zip_reader_get_num_files(&zip.z);
  check(count <= limits::entries, "Too many ZIP entries");
  limits::Inventory inventory;
  std::set<std::string> names;
  std::vector<std::string> paths;
  for (mz_uint i = 0; i < count; ++i) {
    cancelled(progress);
    mz_zip_archive_file_stat s{};
    check(mz_zip_reader_file_stat(&zip.z, i, &s), "Invalid ZIP entry");
    const auto length = mz_zip_reader_get_filename(&zip.z, i, nullptr, 0);
    check(limits::zipFilenameLength(length), "Invalid ZIP filename length");
    std::string name(length, '\0');
    mz_zip_reader_get_filename(&zip.z, i, name.data(), length);
    name.resize(length - 1);
    check(safeRelative(name), "Unsafe ZIP path: " + name);
    check(s.m_is_supported && !s.m_is_encrypted,
          "Unsupported or encrypted ZIP entry");
    check(((s.m_external_attr >> 16) & 0170000) != 0120000,
          "ZIP links are not supported");
    check(names.insert(key(fs::u8path(name))).second,
          "Duplicate ZIP path: " + name);
    inventory.add(s.m_uncomp_size);
    paths.push_back(name);
  }
  check(fs::space(stage).available > inventory.bytes + 1024 * 1024,
        "Not enough free space to import");
  progress.total = count;
  progress.done = 0;
  struct Sink {
    std::ofstream stream;
    Progress *progress;
    uint64_t offset{};
  };
  for (mz_uint i = 0; i < count; ++i) {
    cancelled(progress);
    auto dest = longPath(stage / fs::u8path(paths[i]));
    if (mz_zip_reader_is_file_a_directory(&zip.z, i))
      fs::create_directories(dest);
    else {
      fs::create_directories(dest.parent_path());
      // The filesystem can equate names our portable ASCII key does not
      // (Unicode case pairs, Windows short names). Never truncate an earlier
      // entry even though extraction is confined to a new staging folder.
      check(!fs::exists(dest), "Duplicate ZIP filesystem path: " + paths[i]);
      Sink sink{std::ofstream(dest, std::ios::binary), &progress};
      check(bool(sink.stream), "Cannot extract " + paths[i]);
      check(mz_zip_reader_extract_to_callback(
                &zip.z, i,
                [](void *ptr, mz_uint64 offset, const void *data,
                   size_t n) -> size_t {
                  auto &s = *static_cast<Sink *>(ptr);
                  if (s.progress->cancel || offset != s.offset)
                    return 0;
                  s.stream.write(static_cast<const char *>(data), n);
                  if (!s.stream)
                    return 0;
                  s.offset += n;
                  return n;
                },
                &sink, 0),
            progress.cancel ? "Cancelled" : "Damaged ZIP entry: " + paths[i]);
      sink.stream.flush();
      check(bool(sink.stream), "Cannot finish extracting " + paths[i]);
      sink.stream.close();
      check(bool(sink.stream), "Cannot close extracted file " + paths[i]);
    }
    ++progress.done;
  }
  cancelled(progress);
  const bool managed = fs::exists(stage / "cadence-pack.txt");
  bool hasAssets = false;
  if (managed) {
    int mode{};
    auto aliases = readManifest(stage / "cadence-pack.txt", &mode, &progress);
    for (const auto &[a, b] : aliases) {
      cancelled(progress);
      hasAssets |= fs::is_regular_file(longPath(stage / fs::u8path(b)));
    }
    check(mode != 0 || !hasAssets, "Listing pack unexpectedly contains assets");
    if (hasAssets || mode == 1)
      for (const auto &[a, b] : aliases) {
        cancelled(progress);
        check(fs::is_regular_file(longPath(stage / fs::u8path(b))),
              "Incomplete pack: " + b);
      }
  } else {
    std::map<std::string, std::string> aliases;
    for (const auto &e : fs::recursive_directory_iterator(stage)) {
      cancelled(progress);
      if (e.is_regular_file()) {
        auto relative = utf8(e.path().lexically_relative(stage));
        auto logical = unscopedExportKey(e.path().lexically_relative(stage));
        check(!logical.empty()||!hasExportMarker(e.path().lexically_relative(stage)),
              "Ambiguous export folders in ZIP: " + relative);
        if (logical.empty()) {
          auto slash = relative.find('/');
          if (slash != std::string::npos)
            logical = relative;
        }
        if (!logical.empty()) {
          const auto [unused, inserted] = aliases.emplace(
              "game:" + key(fs::u8path(logical)),
              "assets/exported_files/" + logical);
          check(inserted, "Conflicting asset paths in ZIP: " + logical);
        }
      }
    }
    check(!aliases.empty(), "ZIP needs game folders (for example bo2/models) "
                            "or exported_files/game folders");
    auto normalized = stage / ".normalized";
    check(!fs::exists(normalized), "Reserved pack folder");
    std::vector<std::pair<fs::path, fs::path>> moves;
    for (const auto &e : fs::recursive_directory_iterator(stage)) {
      cancelled(progress);
      if (!e.is_regular_file())
        continue;
      auto rel = utf8(e.path().lexically_relative(stage));
      auto logical = unscopedExportKey(e.path().lexically_relative(stage));
      if (logical.empty())
        logical = rel;
      if (auto a = aliases.find("game:" + key(fs::u8path(logical)));
          a != aliases.end())
        moves.push_back(
            {e.path(), longPath(normalized / fs::u8path(a->second))});
    }
    fs::create_directory(normalized);
    for (const auto &[source, dest] : moves) {
      cancelled(progress);
      fs::create_directories(dest.parent_path());
      fs::rename(source, dest);
    }
    // Only normalized assets are activated; arbitrary top-level files are
    // inert. This is exclusively the newly extracted staging directory. No
    // installed pack or source export is overwritten during normalization.
    if (fs::exists(stage / "assets"))
      fs::remove_all(stage / "assets");
    fs::rename(normalized / "assets", stage / "assets");
    Plan p;
    p.aliases = std::move(aliases);
    writeText(stage / "cadence-pack.txt", manifest(p, true, &progress));
    hasAssets = true;
  }
  auto name = utf8(archive.stem());
  if (!validName(name))
    name = "Imported pack";
  auto destination = unique(longPath(home / "packs"), name);
  auto id = utf8(destination.filename());
  auto demos = listDemos(stage / "demos");
  fs::path newSet;
  Cleanup setCleanup{};
  if (!demos.empty()) {
    fs::create_directories(longPath(home / "demos"));
    newSet = unique(longPath(home / "demos"), name);
    setCleanup.path = longPath(home / "demos" / (".install-" + token()));
    fs::create_directory(setCleanup.path);
    for (const auto &demo : demos) {
      cancelled(progress);
      take::Take t;
      std::string error;
      check(take::load(demo, t, error), "Invalid packed demo: " + error);
      auto m = readMetadata(demo);
      m.pack = hasAssets ? id : std::string{};
      auto dest = setCleanup.path / demo.filename();
      fs::rename(demo, dest);
      writeMetadata(dest, m);
    }
  }
  cancelled(progress);
  fs::rename(stage, destination);
  cleanup.committed = true;
  Cleanup publishedPack{destination};
  if (!newSet.empty())
    fs::rename(setCleanup.path, newSet);
  publishedPack.committed = true;
  setCleanup.committed = true;
  return id;
}
std::vector<Pack> packs(const fs::path &home) {
  std::vector<Pack> result;
  if (!fs::is_directory(home / "packs"))
    return result;
  std::map<std::string, std::vector<size_t>> owners;
  for (const auto &e : fs::directory_iterator(longPath(home / "packs")))
    if (e.is_directory() && !e.path().filename().string().starts_with('.') &&
        fs::exists(e.path() / "cadence-pack.txt"))
      result.push_back({utf8(e.path().filename()), e.path(),
                        !fs::exists(e.path() / "disabled")});
  std::sort(result.begin(), result.end(),
            [](const auto &a, const auto &b) { return a.id < b.id; });
  for (size_t i = 0; i < result.size(); ++i) {
    auto &p = result[i];
    try {
      std::set<std::string> files;
      int mode{};
      const auto aliases =
          readManifest(p.directory / "cadence-pack.txt", &mode);
      const bool anyAssets =
          std::any_of(aliases.begin(), aliases.end(), [&](const auto &entry) {
            return fs::is_regular_file(
                longPath(p.directory / fs::u8path(entry.second)));
          });
      if (mode == 1 || (mode == -1 && anyAssets))
        for (const auto &[a, b] : aliases)
          check(fs::is_regular_file(longPath(p.directory / fs::u8path(b))),
                "Incomplete pack: " + b);
      for (const auto &[a, b] : aliases)
        if (fs::is_regular_file(longPath(p.directory / fs::u8path(b)))) {
          if (files.insert(b).second) {
            ++p.files;
            const auto ext = key(fs::u8path(b).extension());
            if (ext == ".glb" || ext == ".gltf" || ext == ".c2m")
              p.maps.push_back(longPath(p.directory / fs::u8path(b)));
          }
          if (p.enabled && (a.starts_with("game:") || a.starts_with("app:"))) {
            owners[a].push_back(i);
          }
        }
    } catch (const std::exception &e) {
      p.error = e.what();
    }
  }
  for (const auto &[path, candidates] : owners) {
    Pack::Overlap overlap;
    overlap.path = path;
    for (const auto i : candidates)
      if (result[i].error.empty())
        overlap.packs.push_back(result[i].id);
    if (overlap.packs.size() < 2)
      continue;
    overlap.defaultPack = overlap.packs.front();
    for (const auto i : candidates)
      if (result[i].error.empty()) {
        result[i].overlaps.push_back(overlap);
        ++result[i].conflicts;
      }
  }
  return result;
}
void setEnabled(const Pack &pack, bool enabled) {
  if (enabled) {
    std::error_code ec;
    fs::remove(pack.directory / "disabled", ec);
    check(!ec, "Cannot enable pack");
  } else
    writeText(pack.directory / "disabled", "disabled\n");
}
std::vector<fs::path> activatePacks(const fs::path &home) {
  std::vector<PathMount> mounts;
  std::vector<fs::path> roots;
  std::vector<std::string> disabled;
  for (const auto &p : packs(home))
    if (p.enabled && p.error.empty()) {
      PathMount m;
      m.id = p.id;
      m.root = p.directory;
      for (const auto &[a, b] :
           readManifest(p.directory / "cadence-pack.txt")) {
        auto target = longPath(p.directory / fs::u8path(b));
        if (fs::is_regular_file(target))
          m.paths.emplace(a, target);
      }
      mounts.push_back(std::move(m));
      auto root = p.directory / "assets" / "exported_files";
      if (fs::is_directory(root))
        roots.push_back(root);
    } else
      disabled.push_back(key(p.directory));
  setDisabledRoots(std::move(disabled));
  mountPaths(std::move(mounts));
  return roots;
}
} // namespace cadence::content
