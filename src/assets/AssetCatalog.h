#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace assets {

enum class Role { PlayerModel, ViewHands, WorldWeapon, ViewWeapon, WorldAttachment, ViewAttachment, OtherModel, Count };

struct Asset {
    std::filesystem::path path;
    std::string name;
    std::string game;
    Role role{Role::OtherModel};
    std::string category;
    std::vector<std::string> compatibilityKeys;
    std::filesystem::path customManifest;
    std::string customPackageId;
};

struct Catalog {
    std::filesystem::path root;
    std::vector<Asset> entries;
    std::array<std::size_t,static_cast<std::size_t>(Role::Count)> counts{};
    std::size_t skippedLods{};
    std::size_t scannedCastFiles{};

    void clear();
    [[nodiscard]] std::vector<std::size_t> compatibleAttachments(std::size_t weaponIndex) const;
};

[[nodiscard]] Role classifyModelName(std::string_view name);
[[nodiscard]] std::string coldWarWeaponFamily(std::string_view name);
[[nodiscard]] std::string coldWarWeaponVariant(std::string_view name);
[[nodiscard]] Role classifyModelPath(const std::filesystem::path& path, std::string_view stem);
[[nodiscard]] std::string categoryFromPath(const std::filesystem::path& path);
[[nodiscard]] std::vector<std::string> compatibilityKeys(std::string_view name,Role role);
[[nodiscard]] bool isDefaultIwRigPart(std::string_view weaponName,std::string_view partName);
[[nodiscard]] bool isDefaultColdWarRigPart(std::string_view weaponName,std::string_view partName);
// Select one skin-matched magazine/scope, with the family's default as fallback.
[[nodiscard]] bool isPreferredColdWarRigPart(const Catalog& catalog,const Asset& weapon,const Asset& part);
[[nodiscard]] bool scan(const std::filesystem::path& root,Catalog& catalog,std::string& error);
[[nodiscard]] bool appendScan(const std::filesystem::path& root,std::string_view explicitGame,Catalog& catalog,std::string& error);
[[nodiscard]] const char* roleName(Role role);
[[nodiscard]] std::string viewhandsGameForLoadedBase(const Catalog& catalog,std::size_t selectedBaseAsset,const std::filesystem::path& loadedBaseModelPath);

} // namespace assets
