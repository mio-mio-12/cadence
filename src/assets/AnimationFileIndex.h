#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace assets {

// File discovery only: no weapon aliases, ranking or parsed/retargeted clips.
// Owned by one class-load worker and discarded when that request completes.
// Directory timestamps invalidate nested additions/removals between slot scans;
// document contents are always read by the caller, never cached here.
class AnimationFileIndex {
public:
    struct Entry {
        std::filesystem::path path;
        std::string lowerFilename;
        std::vector<std::string> lowerParents;
    };

    const std::vector<Entry>& files(const std::filesystem::path& folder) {
        auto& index = folders_[folder];
        if (index.complete && unchanged(index)) return index.files;
        index = {};
        std::error_code error;
        bool complete = stampDirectory(index, folder);
        for (std::filesystem::recursive_directory_iterator it(folder, std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end; it.increment(error)) {
            if (error) { complete = false; error.clear(); continue; }
            if (it->is_directory(error)) complete = stampDirectory(index, it->path()) && complete;
            if (error) { complete = false; error.clear(); }
            if (it->is_regular_file(error) && it->path().extension() == ".cast") {
                Entry entry{it->path(), lower(it->path().filename().string()), {}};
                for (auto parent = entry.path.parent_path(); parent != folder && !parent.empty(); parent = parent.parent_path())
                    entry.lowerParents.push_back(lower(parent.filename().string()));
                index.files.push_back(std::move(entry));
            }
            if (error) { complete = false; error.clear(); }
        }
        if (error) complete = false;
        std::sort(index.files.begin(), index.files.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
        index.complete = complete;
        ++scans_;
        return index.files;
    }

    static bool matches(const Entry& entry, const std::string& prefix, const std::vector<std::string>& keys) {
        const auto& name = entry.lowerFilename;
        if (!prefix.empty() && name.starts_with(prefix)) return true;
        for (const auto& key : keys)
            if (name.find("_" + key + "_") != std::string::npos || name.starts_with("viewmodel_" + key + "_") ||
                name.starts_with("vm_" + key + "_") || name.ends_with("_" + key + ".cast") || name.ends_with("_" + key + "_model.cast")) return true;
        for (const auto& parent : entry.lowerParents)
            for (const auto& key : keys)
                if (parent == key || parent == prefix || parent == "knife_" + key || parent == "weapon_" + key ||
                    parent == "viewmodel_" + key || parent == "vm_" + key) return true;
        return false;
    }

    std::size_t scans() const { return scans_; }

private:
    struct Folder {
        std::vector<Entry> files;
        std::map<std::filesystem::path, std::filesystem::file_time_type> directories;
        bool complete{};
    };
    static std::string lower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }
    static bool stampDirectory(Folder& index, const std::filesystem::path& path) {
        std::error_code error;
        const auto stamp = std::filesystem::last_write_time(path, error);
        if (error) return false;
        index.directories.emplace(path, stamp);
        return true;
    }
    static bool unchanged(const Folder& index) {
        for (const auto& [path, stamp] : index.directories) {
            std::error_code error;
            const auto current = std::filesystem::last_write_time(path, error);
            if (error || current != stamp) return false;
        }
        return true;
    }
    std::map<std::filesystem::path, Folder> folders_;
    std::size_t scans_{};
};

} // namespace assets
