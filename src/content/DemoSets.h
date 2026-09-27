#pragma once
#include "content/AssetPaths.h"
#include "take/Take.h"
#include <atomic>
#include <functional>
#include <stdexcept>

namespace cadence::content {
namespace limits {
inline constexpr uint64_t entries = 500000;
inline constexpr uint64_t fileBytes = uint64_t(256) * 1024 * 1024 * 1024;
inline constexpr uint64_t archiveBytes = uint64_t(1024) * 1024 * 1024 * 1024;
inline constexpr uint64_t manifestBytes = 64 * 1024 * 1024;
inline constexpr uint64_t aliases = 999999;
inline constexpr size_t pathBytes = 2000;
inline constexpr bool zipFilenameLength(size_t includingNull) {
  return includingNull > 1 && includingNull <= pathBytes + 1;
}
inline void manifest(uint64_t bytes, uint64_t count) {
  if (bytes > manifestBytes || count > aliases)
    throw std::runtime_error("Pack manifest too large");
}
struct Inventory {
  uint64_t count{}, bytes{};
  void add(uint64_t size) {
    if (count >= entries) throw std::runtime_error("Too many ZIP entries");
    if (size > fileBytes || bytes > archiveBytes - size)
      throw std::runtime_error("Archive is too large");
    ++count;
    bytes += size;
  }
};
} // namespace limits
struct Metadata {
  std::string map, pack;
  float mapScale{1};
  std::vector<std::string> construction;
};
struct Progress {
  std::atomic<bool> cancel{false};
  std::atomic<size_t> done{0}, total{0};
};
struct Entry {
  fs::path source;
  std::string destination;
  uintmax_t size{};
  fs::file_time_type modified;
};
struct Plan {
  std::vector<Entry> files;
  std::map<std::string, std::string> aliases;
  std::vector<std::string> missing;
  uintmax_t bytes{};
  std::string setName;
  fs::path sourceSet;
  std::vector<fs::path> reviewedDemos;
};
struct Pack {
  struct Overlap {
    std::string path, defaultPack;
    std::vector<std::string> packs;
  };
  std::string id;
  fs::path directory;
  bool enabled{true};
  size_t files{}, conflicts{};
  std::vector<fs::path> maps;
  std::string error;
  std::vector<Overlap> overlaps;
};
bool safeRelative(std::string name);
bool validName(const std::string &name);
fs::path metadataPath(const fs::path &demo);
Metadata readMetadata(const fs::path &demo);
void writeMetadata(const fs::path &demo, const Metadata &metadata);
std::vector<fs::path> listSets(const fs::path &home);
std::vector<fs::path> listDemos(const fs::path &set);
fs::path createSet(const fs::path &home, const std::string &name);
void saveDemo(const take::Take &take, const Metadata &metadata,
              const fs::path &set, const std::string &name);
void importDemo(const fs::path &source, const fs::path &set);
Plan planSet(const fs::path &set, Progress &progress);
void packSet(const Plan &plan, const fs::path &archive, bool assets,
             Progress &progress);
std::string importPack(const fs::path &home, const fs::path &archive,
                       Progress &progress);
std::vector<Pack> packs(const fs::path &home);
void setEnabled(const Pack &pack, bool enabled);
std::vector<fs::path> activatePacks(const fs::path &home);
std::vector<std::string> missingModels(const take::Take &take,
    const std::function<bool(const std::string&)>& available = {});
} // namespace cadence::content
