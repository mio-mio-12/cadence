#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace cadence::content {
namespace fs = std::filesystem;
inline std::string utf8(const fs::path &p) {
  auto s = p.generic_u8string();
  std::string out{s.begin(), s.end()};
  if (out.starts_with("//?/UNC/"))
    out = "//" + out.substr(8);
  else if (out.starts_with("//?/"))
    out.erase(0, 4);
  return out;
}
inline fs::path longPath(const fs::path &p) {
#ifdef _WIN32
  auto normalized = fs::absolute(p).lexically_normal();
  normalized.make_preferred();
  auto s = normalized.wstring();
  if (s.starts_with(L"\\\\?\\"))
    return normalized;
  if (s.starts_with(L"\\\\"))
    return fs::path(L"\\\\?\\UNC\\" + s.substr(2));
  return fs::path(L"\\\\?\\" + s);
#else
  return p;
#endif
}
inline std::string key(const fs::path &p) {
  auto s = utf8(p.lexically_normal());
  std::replace(s.begin(), s.end(), '\\', '/');
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}
inline bool hasExportMarker(const fs::path& p) {
  const auto s="/"+key(p);
  return s.find("/exported_files/")!=std::string::npos||s.find("/_files/")!=std::string::npos;
}
// Without an authoritative root, two export markers are ambiguous: neither
// the first nor last can safely be assumed to identify the actual game.
inline std::string unscopedExportKey(const fs::path &p) {
  const auto s="/"+key(p);std::string result;size_t count{};
  for (const auto marker : {"/exported_files/", "/_files/"})
    for(size_t n=s.find(marker);n!=std::string::npos;n=s.find(marker,n+1)){
      result=s.substr(n+std::char_traits<char>::length(marker));++count;
    }
  return count==1?result:std::string{};
}
inline std::string appKey(const fs::path &p) {
  auto s = key(p);
  auto n = s.find("/cadence assets/");
  return n == std::string::npos ? std::string{} : s.substr(n + 16);
}
struct PathMount {
  std::string id;
  std::map<std::string, fs::path> paths;
  fs::path root;
};
inline std::mutex pathMutex;
inline std::vector<PathMount> pathMounts;
inline std::set<std::string> mountedFiles;
inline std::map<std::string, std::vector<std::string>> mountedAliases;
inline std::string preferredPack;
inline fs::path fallbackExportRoot;
inline std::vector<std::string> disabledRoots;
inline std::uint64_t resolverRevision{};
// Caller owns pathMutex. Mounted roots outrank ancestor directory names and
// global fallback roots; their assets/exported_files layout is authoritative.
inline std::string exportKeyUnlocked(const fs::path& p){
  const auto path=key(p);const PathMount* owner=nullptr;size_t ownerLength{};
  for(const auto& mount:pathMounts)if(!mount.root.empty()){
    const auto root=key(mount.root);
    if(path.starts_with(root+"/")&&root.size()>ownerLength){owner=&mount;ownerLength=root.size();}
  }
  if(owner){
    const auto relative=path.substr(ownerLength+1);constexpr auto prefix="assets/exported_files/";
    if(relative.starts_with(prefix))return relative.substr(std::char_traits<char>::length(prefix));
    // Nonstandard packed files can still have an explicit logical alias.
    std::string logical;
    for(const auto& [alias,target]:owner->paths)if(alias.starts_with("game:")&&key(target)==path){
      const auto next=alias.substr(5);if(!logical.empty()&&logical!=next)return {};logical=next;
    }
    return logical;
  }
  if(!fallbackExportRoot.empty()){
    const auto root=key(fallbackExportRoot);
    if(path.starts_with(root+"/"))return path.substr(root.size()+1);
  }
  return unscopedExportKey(p);
}
inline std::string exportKey(const fs::path& p){std::lock_guard lock(pathMutex);return exportKeyUnlocked(p);}
inline void setDisabledRoots(std::vector<std::string> roots) {
  std::lock_guard lock(pathMutex);
  disabledRoots = std::move(roots);
  ++resolverRevision;
}
inline void setFallbackExportRoot(fs::path root) {
  std::lock_guard lock(pathMutex);
  fallbackExportRoot = std::move(root);
  ++resolverRevision;
}
inline thread_local std::string resolutionPack;
inline thread_local bool resolutionScoped = false;
// Identity only: no documents or resolved results outlive an assembly request.
inline std::string resolverContextKey() {
  std::lock_guard lock(pathMutex);
  return std::to_string(resolverRevision) + (resolutionScoped ? ":scope:" : ":preferred:") +
         (resolutionScoped ? resolutionPack : preferredPack);
}
struct ResolutionScope {
  std::string previous;
  bool previouslyScoped = resolutionScoped;
  explicit ResolutionScope(std::string id)
      : previous(std::move(resolutionPack)) {
    resolutionPack = std::move(id);
    resolutionScoped = true;
  }
  ~ResolutionScope() {
    resolutionPack = std::move(previous);
    resolutionScoped = previouslyScoped;
  }
};
inline std::set<fs::path> observedDocuments;
inline thread_local bool observeEnabled = true;
inline thread_local std::set<fs::path>* observationSink = nullptr;
struct ScopedDocumentCollection {
  std::set<fs::path>* previous = observationSink;
  explicit ScopedDocumentCollection(std::set<fs::path>& destination) {
    observationSink = &destination;
  }
  ~ScopedDocumentCollection() { observationSink = previous; }
  ScopedDocumentCollection(const ScopedDocumentCollection&) = delete;
  ScopedDocumentCollection& operator=(const ScopedDocumentCollection&) = delete;
};
struct ObservationPause {
  bool previous = observeEnabled;
  ObservationPause() { observeEnabled = false; }
  ~ObservationPause() { observeEnabled = previous; }
};
inline void observeDocument(const fs::path &p) {
  // Library discovery probes deliberately pause dependency observation. Keep
  // that behavior inside a request-local collector as well.
  if(!observeEnabled)return;
  if(observationSink){observationSink->insert(p);return;}
  std::lock_guard lock(pathMutex);
  observedDocuments.insert(p);
}
inline std::vector<fs::path> constructionDocuments() {
  std::lock_guard lock(pathMutex);
  return {observedDocuments.begin(), observedDocuments.end()};
}
inline void mountPaths(std::vector<PathMount> mounts) {
  std::lock_guard lock(pathMutex);
  pathMounts = std::move(mounts);
  ++resolverRevision;
  mountedFiles.clear();
  mountedAliases.clear();
  for (const auto &m : pathMounts)
    for (const auto &[k, p] : m.paths) {
      auto path = key(p);
      mountedFiles.insert(path);
      mountedAliases[path].push_back(k);
    }
}
inline bool packActive(const std::string &id) {
  std::lock_guard lock(pathMutex);
  return std::any_of(pathMounts.begin(), pathMounts.end(),
                     [&](const auto &m) { return m.id == id; });
}
inline std::vector<std::string> aliasesFor(const fs::path &path) {
  std::lock_guard lock(pathMutex);
  auto i = mountedAliases.find(key(path));
  return i == mountedAliases.end() ? std::vector<std::string>{} : i->second;
}
inline void preferPack(std::string id) {
  std::lock_guard lock(pathMutex);
  preferredPack = std::move(id);
  ++resolverRevision;
}
inline fs::path resolve(fs::path source) {
#ifdef _WIN32
  if (source.native().starts_with(L"\\\\?\\"))
    source.make_preferred();
#endif
  if (source.empty())
    return source;
  std::lock_guard lock(pathMutex);
  const auto exact = key(source), logical = exportKeyUnlocked(source),
             app = appKey(source);
  for (const auto &root : disabledRoots)
    if (exact.starts_with(root + "/"))
      return {};
  const auto find = [&](const PathMount &m) -> fs::path {
    for (const auto &k :
         {exact, logical.empty() ? std::string{} : "game:" + logical,
          app.empty() ? std::string{} : "app:" + app})
      if (!k.empty())
        if (auto i = m.paths.find(k); i != m.paths.end())
          return i->second;
    return {};
  };
  // Keep already-mounted references in their own namespace.
  if (mountedFiles.contains(exact))
    return source;
  for (const auto &m : pathMounts)
    if (m.id == (resolutionScoped ? resolutionPack : preferredPack))
      if (auto p = find(m); !p.empty())
        return p;
  for (const auto &m : pathMounts)
    if (auto p = find(m); !p.empty())
      return p;
  if (!logical.empty() && !fallbackExportRoot.empty()) {
    std::error_code ec;
    if (fs::is_regular_file(source, ec))
      return source;
    auto local = fallbackExportRoot / fs::u8path(logical);
    if (fs::is_regular_file(local, ec))
      return local;
  }
  return source;
}
inline fs::path resolveFor(const fs::path &source, const fs::path &context) {
  std::string owner;
  {
    std::lock_guard lock(pathMutex);
    const auto k = key(context);
    for (const auto &m : pathMounts)
      if (!m.root.empty() && k.starts_with(key(m.root) + "/")) {
        owner = m.id;
        break;
      }
  }
  if (owner.empty())
    return resolve(source);
  ResolutionScope scope(owner);
  return resolve(source);
}
} // namespace cadence::content
