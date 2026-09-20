#pragma once
#include <cstdlib>
#include <filesystem>
#include <string>

namespace cadence::local_assets {
// Optional asset-dependent diagnostics use an explicit environment override,
// or an exported_files directory relative to their working directory.
inline std::string exportPath(const char* relative="") {
    const char* configured=std::getenv("CADENCE_TEST_ASSETS");
    const auto root=configured&&*configured?std::filesystem::u8path(configured):std::filesystem::path{"exported_files"};
    return (root/std::filesystem::u8path(relative)).string();
}
}
