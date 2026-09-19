#pragma once
#include "scene/BoundedJson.h"
#include "scene/CastScene.h"

namespace scene::codm {
// Runtime fields are validated before mesh mutation. Raw shader recipes remain preserved.
bool applyCodmMaterial(const Json& record, const std::filesystem::path& packageDirectory,
                       Mesh& mesh, std::string& error, bool glbApproximation=false);
bool validateCodmMaterials(const Json& records, const std::filesystem::path& packageDirectory,
                           std::string& error);
std::filesystem::path packageTexturePath(const std::filesystem::path& directory,
                                        const std::string& relative);
}
