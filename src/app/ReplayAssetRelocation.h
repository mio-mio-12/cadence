#pragma once
#include "assets/AssetCatalog.h"
#include "content/AssetPaths.h"
#include <optional>

namespace cadence::replay {
inline std::string storedGame(const std::filesystem::path& path) {
  const auto logical=content::exportKey(path);
  const auto slash=logical.find('/');
  return slash==std::string::npos?std::string{}:logical.substr(0,slash);
}
// Exact mounts/local paths retain precedence. Relocation alone must never pick
// an arbitrary rig when several games/exports reuse the same basename.
inline std::optional<std::filesystem::path> relocateModel(
    const std::string& stored,const assets::Catalog& catalog,
    const std::filesystem::path& exportRoot) {
  namespace fs=std::filesystem;
  if(stored.empty())return {};
  const auto original=fs::u8path(stored);
  const auto exact=content::resolve(original);
  if(exact.empty())return {}; // explicitly disabled pack namespace
  std::error_code ec;
  if(fs::is_regular_file(exact,ec))return exact;
  const auto game=storedGame(original),filename=content::key(original.filename()),stem=content::key(original.stem());
  struct Candidates {
    std::optional<fs::path> value;
    fs::path identity;
    bool ambiguous{};
    void add(const fs::path& source) {
      const auto path=content::resolve(source);
      std::error_code error;
      if(path.empty()||!fs::is_regular_file(path,error))return;
      auto canonical=fs::weakly_canonical(path,error);
      if(error)canonical=path.lexically_normal();
      // Canonical paths are only identity keys. Returning one changes catalog
      // spelling (notably Windows long-path prefixes), breaking downstream
      // path-to-game lookup used to prepare foreign viewhands.
      if(!value){value=path;identity=canonical;return;}
      if(identity==canonical)return;
      error.clear();if(fs::equivalent(identity,canonical,error)&&!error)return;
      ambiguous=true;
    }
  };
  for(bool byStem:{false,true}) {
    Candidates matches;
    for(const auto& asset:catalog.entries) {
      if(!game.empty()&&content::key(fs::u8path(asset.game))!=game)continue;
      if(content::key(byStem?asset.path.stem():asset.path.filename())==(byStem?stem:filename))matches.add(asset.path);
    }
    if(matches.ambiguous)return {};
    if(matches.value)return matches.value;
  }
  if(exportRoot.empty()||!fs::is_directory(exportRoot,ec))return {};
  Candidates matches;
  for(fs::recursive_directory_iterator it(exportRoot,fs::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)) {
    if(ec){ec.clear();continue;}
    if(content::key(it->path().filename())!=filename||!it->is_regular_file(ec))continue;
    if(!game.empty()) {
      auto candidateGame=storedGame(it->path());
      if(candidateGame.empty()) {
        const auto relative=it->path().lexically_relative(exportRoot);
        if(relative.begin()!=relative.end())candidateGame=content::key(*relative.begin());
      }
      if(candidateGame!=game)continue;
    }
    matches.add(it->path());
    if(matches.ambiguous)return {};
  }
  return matches.value;
}
} // namespace cadence::replay
