#pragma once
#include "content/DemoSets.h"
#include <array>
#include <future>
#include <optional>
namespace cadence::content {
struct JobResult {
  std::string message;
  std::optional<Plan> plan;
  bool success{};
  bool requiresRestart{};
};
struct DemoLibrary {
  Metadata recording;
  std::vector<fs::path> roots, sets, demos;
  std::map<fs::path, Metadata> demoInfo;
  std::vector<Pack> installed;
  std::map<std::string, size_t> overlapSelection;
  fs::path set, pendingDemo, pendingMap;
  float pendingMapScale{1};
  std::array<char, 128> setName{}, demoName{};
  std::optional<Plan> plan;
  std::optional<std::future<JobResult>> job;
  std::shared_ptr<Progress> progress = std::make_shared<Progress>();
  bool restart{}, initialized{}, open{};
  ~DemoLibrary() {
    progress->cancel = true;
    if (job)
      job->wait();
  }
};
} // namespace cadence::content
