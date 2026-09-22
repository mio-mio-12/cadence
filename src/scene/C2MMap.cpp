#include "scene/C2MMap.h"
#include "scene/C2mxMap.h"
#include "scene/SpawnSelection.h"
#include "scene/CodmMaterialMetadata.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

namespace scene::c2m {
namespace {

constexpr float kInchesToCm = 2.54f;

struct BinaryReader {
    const std::uint8_t* data{};
    std::size_t size{};
    std::size_t cursor{};

    [[nodiscard]] bool canRead(std::size_t bytes) const {
        return cursor <= size && bytes <= size-cursor;
    }

    void seek(std::size_t offset) {
        if(offset>size)throw std::runtime_error("C2M offset outside base file");cursor=offset;
    }

    std::uint8_t readByte() {
        if (!canRead(1)) throw std::runtime_error("Truncated C2M byte");
        return data[cursor++];
    }

    bool readBool() {
        return readByte() != 0;
    }

    std::int16_t readShort() {
        if (!canRead(2)) throw std::runtime_error("Truncated C2M short");
        std::int16_t val{};
        std::memcpy(&val, data + cursor, 2);
        cursor += 2;
        return val;
    }

    std::uint16_t readUShort() {
        if (!canRead(2)) throw std::runtime_error("Truncated C2M short");
        std::uint16_t val{};
        std::memcpy(&val, data + cursor, 2);
        cursor += 2;
        return val;
    }

    std::int32_t readInt() {
        if (!canRead(4)) throw std::runtime_error("Truncated C2M integer");
        std::int32_t val{};
        std::memcpy(&val, data + cursor, 4);
        cursor += 4;
        return val;
    }

    std::uint32_t readUInt() {
        if (!canRead(4)) throw std::runtime_error("Truncated C2M integer");
        std::uint32_t val{};
        std::memcpy(&val, data + cursor, 4);
        cursor += 4;
        return val;
    }

    float readFloat() {
        if (!canRead(4)) throw std::runtime_error("Truncated C2M float");
        float val{};
        std::memcpy(&val, data + cursor, 4);
        cursor += 4;
        return val;
    }

    std::uint64_t readULong() {
        if (!canRead(8)) throw std::runtime_error("Truncated C2M offset");
        std::uint64_t val{};
        std::memcpy(&val, data + cursor, 8);
        cursor += 8;
        return val;
    }

    std::string readString() {
        if (!canRead(1)) throw std::runtime_error("Truncated C2M string");
        cursor++; // length flag byte
        std::string result;
        while (canRead(1)) {
            char c = static_cast<char>(data[cursor++]);
            if (c == '\0') return result;
            result.push_back(c);
            if(result.size()>1024*1024)throw std::runtime_error("C2M string exceeds bound");
        }
        throw std::runtime_error("Unterminated C2M string");
    }
};

inline std::string_view gameNameForVersion(std::uint8_t ver) {
    switch (ver) {
        case 0: return "modern_warfare";
        case 1: return "world_at_war";
        case 2: return "modern_warfare_2";
        case 3: return "black_ops";
        case 4: return "modern_warfare_3";
        case 5: return "black_ops_2";
        case 6: return "future_warfare";
        case 7: return "ghosts";
        case 8: return "advanced_warfare";
        case 9: return "online";
        case 10: return "black_ops_3";
        case 11: return "modern_warfare_rm";
        case 12: return "infinite_warfare";
        case 13: return "world_war_2";
        case 14: return "black_ops_4";
        case 15: return "modern_warfare_4";
        case 16: return "modern_warfare_2_rm";
        case 17: return "black_ops_5";
        case 18: return "vanguard";
        case 19: return "modern_warfare_5";
        case 20: return "modern_warfare_6";
        case 21: return "black_ops_6";
        case 22: return "black_ops_7";
        default: return "";
    }
}

struct MaterialDef {
    std::string name;
    std::string techset;
    std::string surfType;
    std::uint8_t blending{};
    std::uint8_t sortKey{};
    std::string colorMap;
    std::string normalMap;
    std::string specularMap;
};

struct SurfaceDef {
    std::string name;
    std::uint64_t drawSurf{};
    std::vector<std::uint16_t> materials;
    std::vector<std::array<std::uint32_t, 3>> faces;
};

struct MeshDef {
    std::string name;
    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    std::vector<Vec4> colors;
    std::vector<SurfaceDef> surfaces;
};

struct InstanceDef {
    std::string name;
    Vec3 position{};
    Vec3 rotationDegrees{};
    Vec3 scale{1, 1, 1};
};

std::string canonical(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isAlphaTestedFoliage(std::string_view name) {
    constexpr std::array<std::string_view, 24> tokens = {
        "foliage", "leaf", "leaves", "canopy", "tree", "oak", "acacia", "sway",
        "branch", "bush", "plant", "grass", "palm", "fern", "ivy", "vine",
        "flower", "bougainvillea", "hedge", "needle", "pine", "shrub",
        "bark_canopy", "cypress"
    };
    for (const auto token : tokens) {
        if (name.find(token) != std::string_view::npos) return true;
    }
    return false;
}

bool isDecalSurface(std::string_view name) {
    if (isAlphaTestedFoliage(name)) return false;
    constexpr std::array<std::string_view, 53> tokens = {
        "decal", "dec_", "graffiti", "graphitti", "poster", "paper", "calender",
        "bulletin", "bullet_hole", "bulletmark", "stain", "splatter", "blood_splat",
        "light_hotspot", "lensflare", "godray", "fillet", "flower", "ivy", "vine",
        "display", "screen", "monitor", "tv_", "overlay", "sign_", "signage",
        "plaque", "sticker", "bougainvillea", "accessories_frame",
        "frame10_pic", "painting", "metal_vent", "vent_", "damage_wall",
        "calcium_drip", "glow", "_glow", "logo", "flare", "corona",
        "light_glow", "fx_", "_fx", "neon", "switch", "socket", "patch",
        "tarp", "grate", "drain", "debris"
    };
    for (const auto token : tokens) {
        if (name.find(token) != std::string_view::npos) return true;
    }
    return false;
}

bool isAdditiveSurface(std::string_view name, std::string_view techset) {
    if (name.find("sky") != std::string_view::npos || techset.find("sky") != std::string_view::npos) return false;
    if (name.find("wood") != std::string_view::npos ||
        name.find("steel") != std::string_view::npos ||
        name.find("metal") != std::string_view::npos ||
        name.find("concrete") != std::string_view::npos ||
        name.find("brick") != std::string_view::npos ||
        name.find("stone") != std::string_view::npos ||
        name.find("plaster") != std::string_view::npos ||
        name.find("tile") != std::string_view::npos) {
        return false;
    }
    constexpr std::array<std::string_view, 16> tokens = {
        "godray", "lenseflare", "lensflare", "spotlight", "floodlight",
        "light_beam", "floodlight_beam", "spotlight_beam", "laser_beam", "fx_beam",
        "_beam_godray", "flare", "light_glow", "corona", "additive", "unlit_add"
    };
    for (const auto token : tokens) {
        if (name.find(token) != std::string_view::npos || techset.find(token) != std::string_view::npos) return true;
    }
    return false;
}

bool isToolOrInvisibleMaterial(std::string_view name) {
    constexpr std::array<std::string_view, 26> tokens = {
        "hdrportal", "portal", "tools_caulk", "caulk", "shadowcaster", "shadow_caster",
        "shadow_proxy", "tree_shadow_caster", "tools_nodraw", "nodraw", "clip_player",
        "clip_nosight", "clip_missile", "clip_ai", "clip_glass", "clip_vehicle", "clip_weapon",
        "tools_hint", "trigger", "door_clip", "badplace", "traverse", "script_brushmodel",
        "glass_shatter", "glass_cracked", "invisible"
    };
    for (const auto token : tokens) {
        if (name.find(token) != std::string_view::npos) return true;
    }
    return false;
}

bool isAlphaTransparentMaterial(std::string_view name, std::string_view techset, std::string_view surfType) {
    (void)surfType; // Do not use physical surface sound/footstep type for visual transparency

    // Hard exclusions for standard opaque building geometry and foliage cutouts (foliage is handled via alpha test in Pass 0)
    if (name.find("rock") != std::string_view::npos ||
        name.find("stone") != std::string_view::npos ||
        name.find("dirt") != std::string_view::npos ||
        name.find("concrete") != std::string_view::npos ||
        name.find("marble") != std::string_view::npos ||
        name.find("plaster") != std::string_view::npos ||
        name.find("wood") != std::string_view::npos ||
        name.find("brick") != std::string_view::npos ||
        name.find("metal") != std::string_view::npos ||
        name.find("floor") != std::string_view::npos ||
        name.find("wall") != std::string_view::npos ||
        name.find("tile") != std::string_view::npos ||
        name.find("pavement") != std::string_view::npos ||
        name.find("road") != std::string_view::npos ||
        name.find("asphalt") != std::string_view::npos ||
        isAlphaTestedFoliage(name)) {
        return false;
    }

    constexpr std::array<std::string_view, 8> nameTokens = {
        "glass", "window", "water_pool", "water_fall", "water_stream", "water_surface", "water_caustic", "frosted"
    };
    for (const auto token : nameTokens) {
        if (name.find(token) != std::string_view::npos) return true;
    }

    constexpr std::array<std::string_view, 2> techsetTokens = {
        "mc_glass", "mc_water"
    };
    for (const auto token : techsetTokens) {
        if (techset.find(token) != std::string_view::npos) return true;
    }

    return false;
}

bool isSkyboxMaterial(std::string_view name, std::string_view techset) {
    constexpr std::array<std::string_view, 10> tokens = {
        "sky", "skybox", "sun_flare", "sunflare", "sun_sprite", "corona", "atmosphere", "skydome", "cloud", "fog_volume"
    };
    for (const auto token : tokens) {
        if (name.find(token) != std::string_view::npos || techset.find(token) != std::string_view::npos) return true;
    }
    return false;
}

bool isNonSolidFoliageOrDecal(std::string_view name) {
    if (isAlphaTestedFoliage(name) || isDecalSurface(name)) return true;

    constexpr std::array<std::string_view, 40> nonSolidTokens = {
        "grass", "lightgrass", "foliage", "plant", "jungle", "bush", "shrub",
        "tree_card", "forestcards", "canopy", "leaves", "leaf", "flower", "ivy",
        "vine", "fern", "reed", "weed", "moss", "bark_canopy", "hedge", "card",
        "trash", "debris_paper", "paper", "poster", "clutter", "wires", "cable",
        "rope", "antenna", "cloth_banner", "banner", "flag", "tarp", "godray",
        "shadow", "lensflare", "portal", "light_hotspot"
    };
    for (const auto token : nonSolidTokens) {
        if (name.find(token) != std::string_view::npos) return true;
    }
    return false;
}

bool isDecorativePropModel(std::string_view name) {
    if (isAlphaTestedFoliage(name) || isDecalSurface(name) || isNonSolidFoliageOrDecal(name)) return true;

    constexpr std::string_view tokens[] = {
        "foliage", "drygrass", "litegrass", "pacific_grass", "grass", "tree", "bush", "hedge",
        "plant", "vine", "ivy", "leaves", "leaf", "reed", "weed", "flower", "bougainvillea",
        "cypress", "oak", "acacia", "shrub", "canopy", "forestcards", "tree_card", "birch",
        "litter", "trash", "debris", "rubble", "dust", "paper", "cardboard",
        "pipe", "wire", "cable", "electrical", "airduct", "duct", "bracket", "bolt", "screw", "vent",
        "light", "lamp", "bulb", "lantern", "fixture", "street_light", "chandelier",
        "curtain", "banner", "flag", "cloth", "tarp",
        "monitor", "tv_", "screen", "keyboard", "computer", "camera",
        "bottle", "can", "cup", "plate", "dish", "tray"
    };
    for (const auto token : tokens) {
        if (name.find(token) != std::string_view::npos) return true;
    }
    return false;
}

std::vector<std::string> generateTextureCandidates(const std::string& textureName) {
    std::vector<std::string> candidates;
    if (textureName.empty()) return candidates;
    candidates.push_back(textureName);

    std::string stripped = textureName;
    std::size_t start = 0;
    while (start < stripped.size() && (stripped[start] == '~' || stripped[start] == '-' || stripped[start] == '&' || stripped[start] == '$' || stripped[start] == '!')) {
        start++;
        if (start < stripped.size() && (stripped[start] == 'g' || stripped[start] == 'r' || stripped[start] == 'b' || stripped[start] == 'a' || stripped[start] == '-')) {
            start++;
        }
    }
    if (start > 0 && start < stripped.size()) {
        stripped = stripped.substr(start);
        candidates.push_back(stripped);
    }
    if (const auto tildePos = stripped.find('~'); tildePos != std::string::npos && tildePos > 0) {
        candidates.push_back(stripped.substr(0, tildePos));
    }
    if (const auto rgbPos = stripped.find("-rgb"); rgbPos != std::string::npos && rgbPos > 0) {
        candidates.push_back(stripped.substr(0, rgbPos));
    }
    return candidates;
}

struct TextureIndex {
    std::unordered_map<std::string, std::filesystem::path> files;

    void addDirectory(const std::filesystem::path& dir) {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec)) return;
        for (const auto& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (entry.is_regular_file(ec)) {
                const auto stem = canonical(entry.path().stem().string());
                const auto filename = canonical(entry.path().filename().string());
                files.try_emplace(stem, entry.path().lexically_normal());
                files.try_emplace(filename, entry.path().lexically_normal());
            }
        }
    }
};

std::filesystem::path resolveTextureFile(
    const TextureIndex& index,
    const std::string& textureName)
{
    if (textureName.empty() || textureName == "$identitynormalmap" || textureName == "$white") return {};
    const auto candidates = generateTextureCandidates(textureName);

    for (const auto& cand : candidates) {
        const auto canCand = canonical(cand);
        const auto it = index.files.find(canCand);
        if (it != index.files.end()) {
            return it->second;
        }
    }
    return {};
}

void growBounds(Bounds& bounds, Vec3 p) {
    if (!bounds.valid) {
        bounds.minimum = bounds.maximum = p;
        bounds.valid = true;
        return;
    }
    bounds.minimum = {std::min(bounds.minimum.x, p.x), std::min(bounds.minimum.y, p.y), std::min(bounds.minimum.z, p.z)};
    bounds.maximum = {std::max(bounds.maximum.x, p.x), std::max(bounds.maximum.y, p.y), std::max(bounds.maximum.z, p.z)};
}

} // namespace

static bool loadImpl(
    const std::filesystem::path& path,
    scene::glb::Map& map,
    std::string& error,
    float scaleMultiplier,
    const std::function<void(std::string_view, float)>& onProgress,
    const LoadOptions& options)
{
    error.clear();
    map = {};
    map.scaleMultiplier = scaleMultiplier;
    map.sourcePath = path;

    const auto report = [&](std::string_view msg, float progress) {
        if (onProgress) onProgress(msg, progress);
    };

    report("Opening C2M map file...", 0.05f);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "Could not open C2M file: " + path.filename().string();
        return false;
    }
    const auto fileSize = static_cast<std::size_t>(file.tellg());
    if(fileSize>2ull*1024*1024*1024||fileSize<85){error="Invalid C2M file size";return false;}
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize))) {
        error = "Could not read C2M file data";
        return false;
    }

    const auto extension=c2mx::readExtension(buffer);
    if(extension.present&&!c2mx::applyCollision(extension.collision,extension.meta,extension.navigation,map,error,options,false))return false;
    BinaryReader r{buffer.data(), extension.baseEnd, 0};
    if (r.readByte() != 'C' || r.readByte() != '2' || r.readByte() != 'M') {
        error = "Invalid C2M magic header";
        return false;
    }

    const auto fileVersion = r.readByte();
    const auto versionId = r.readByte();
    const auto mapName = r.readString();
    const auto skybox = r.readString();
    const auto gameName = gameNameForVersion(versionId);

    const auto objectCount = r.readUInt();
    const auto objectOffset = r.readULong();
    const auto staticInstanceCount = r.readUInt();
    const auto dynamicInstanceCount = r.readUInt();
    const auto instanceOffset = r.readULong();
    const auto imageCount = r.readUInt();
    const auto imageOffset = r.readULong();
    const auto materialCount = r.readUInt();
    const auto materialOffset = r.readULong();
    const auto lightCount = r.readUInt();
    const auto lightOffset = r.readULong();
    const auto entsOffset = r.readULong();
    if(objectCount>100000||materialCount>65535||staticInstanceCount>1000000||dynamicInstanceCount>1000000)throw std::runtime_error("C2M table count exceeds bounds");
    if(extension.present){
        if(!extension.meta.contains("visualMaterials")||!extension.meta["visualMaterials"].is_array()||extension.meta["visualMaterials"].size()!=materialCount)throw std::runtime_error("C2MX material count does not match base table");
        if(!codm::validateCodmMaterials(extension.meta["visualMaterials"],path.parent_path(),error))return false;
    }

    report("Reading material table (" + std::to_string(materialCount) + " materials)...", 0.12f);

    // Read Materials
    r.seek(materialOffset);
    std::vector<MaterialDef> materials;
    materials.reserve(materialCount);
    for (std::uint32_t m = 0; m < materialCount; ++m) {
        MaterialDef mat;
        mat.name = r.readString();
        mat.techset = r.readString();
        mat.surfType = r.readString();
        mat.blending = r.readByte();
        mat.sortKey = r.readByte();
        const auto texCount = r.readByte();
        const auto settingsCount = r.readByte();

        std::string firstTexture;
        for (std::uint8_t t = 0; t < texCount; ++t) {
            const auto texName = r.readString();
            const auto texType = r.readString();
            if (firstTexture.empty() && !texName.empty() && !texName.starts_with("$")) {
                firstTexture = texName;
            }
            const auto canType = canonical(texType);
            if (canType.find("color") != std::string::npos || canType.find("diffuse") != std::string::npos || canType.find("layer1") != std::string::npos) {
                if (mat.colorMap.empty()) mat.colorMap = texName;
            } else if (canType.find("normal") != std::string::npos || canType.find("bump") != std::string::npos || canType.find("nml") != std::string::npos) {
                if (mat.normalMap.empty()) mat.normalMap = texName;
            } else if (canType.find("spec") != std::string::npos || canType.find("gloss") != std::string::npos || canType.find("rough") != std::string::npos) {
                if (mat.specularMap.empty()) mat.specularMap = texName;
            }
        }
        if (mat.colorMap.empty() && !firstTexture.empty()) {
            mat.colorMap = firstTexture;
        }

        for (std::uint8_t s = 0; s < settingsCount; ++s) {
            r.readString();
            r.readUInt();
            r.readByte();
            r.readFloat(); r.readFloat(); r.readFloat(); r.readFloat();
        }
        materials.push_back(std::move(mat));
    }

    report("Parsing " + std::to_string(objectCount) + " mesh objects...", 0.25f);

    // Read Objects
    r.seek(objectOffset);
    std::vector<MeshDef> objects;
    std::unordered_map<std::string, std::size_t> objectIndexByName;
    objects.reserve(objectCount);

    for (std::uint32_t o = 0; o < objectCount; ++o) {
        MeshDef mesh;
        mesh.name = r.readString();
        const auto vertexCount = r.readUInt();
        const auto surfCount = r.readUInt();
        const auto faceCount = r.readUInt();
        const auto lodCount = r.readUInt();
        const auto lodDistance = r.readFloat();
        if(vertexCount>10000000||surfCount>1000000||faceCount>20000000||lodCount>64||!r.canRead(static_cast<std::size_t>(vertexCount)*12))throw std::runtime_error("C2M geometry count/bounds invalid");

        mesh.vertices.resize(vertexCount);
        for (std::uint32_t v = 0; v < vertexCount; ++v) {
            mesh.vertices[v] = {r.readFloat(), r.readFloat(), r.readFloat()};
            if(extension.present&&(!std::isfinite(mesh.vertices[v].x)||!std::isfinite(mesh.vertices[v].y)||!std::isfinite(mesh.vertices[v].z)))throw std::runtime_error("Nonfinite C2MX visual vertex");
        }

        mesh.normals.resize(vertexCount);
        for (std::uint32_t v = 0; v < vertexCount; ++v) {
            mesh.normals[v] = {r.readFloat(), r.readFloat(), r.readFloat()};
        }

        mesh.uvs.resize(vertexCount);
        for (std::uint32_t v = 0; v < vertexCount; ++v) {
            const auto uvSetCount = r.readUInt();
            if(uvSetCount>64||!r.canRead(static_cast<std::size_t>(uvSetCount)*8))throw std::runtime_error("Invalid C2M UV array");
            for (std::uint32_t u = 0; u < uvSetCount; ++u) {
                float uCoord = r.readFloat();
                float vCoord = r.readFloat();
                if (u == 0) mesh.uvs[v] = {uCoord, vCoord};
            }
        }

        mesh.colors.resize(vertexCount);
        for (std::uint32_t v = 0; v < vertexCount; ++v) {
            mesh.colors[v] = {
                r.readByte() / 255.0f,
                r.readByte() / 255.0f,
                r.readByte() / 255.0f,
                r.readByte() / 255.0f
            };
        }

        mesh.surfaces.resize(surfCount);
        for (std::uint32_t s = 0; s < surfCount; ++s) {
            auto& surf = mesh.surfaces[s];
            surf.name = r.readString();
            surf.drawSurf = r.readULong();
            const auto uvLayerCount = r.readByte();
            const auto matCount = r.readByte();
            surf.materials.resize(matCount);
            for (std::uint8_t m = 0; m < matCount; ++m) {
                surf.materials[m] = r.readUShort();
                if(extension.present&&surf.materials[m]>=materials.size())throw std::runtime_error("C2MX surface material index out of range");
            }
            const auto surfFaces = r.readUInt();
            if(surfFaces>20000000||!r.canRead(static_cast<std::size_t>(surfFaces)*12))throw std::runtime_error("Invalid C2M surface face bounds");
            surf.faces.resize(surfFaces);
            for (std::uint32_t f = 0; f < surfFaces; ++f) {
                surf.faces[f] = {r.readUInt(), r.readUInt(), r.readUInt()};
                if(extension.present&&(surf.faces[f][0]>=vertexCount||surf.faces[f][1]>=vertexCount||surf.faces[f][2]>=vertexCount))throw std::runtime_error("C2MX visual triangle index out of range");
            }
        }

        // Skip LOD definitions
        for (std::uint32_t l = 0; l < lodCount; ++l) {
            const auto lodVerts = r.readUInt();
            const auto lodSurfs = r.readUInt();
            const auto lodFaces = r.readUInt();
            r.readFloat();
            for (std::uint32_t v = 0; v < lodVerts * 3; ++v) r.readFloat();
            for (std::uint32_t v = 0; v < lodVerts * 3; ++v) r.readFloat();
            for (std::uint32_t v = 0; v < lodVerts; ++v) {
                const auto sets = r.readUInt();
                for (std::uint32_t u = 0; u < sets * 2; ++u) r.readFloat();
            }
            for (std::uint32_t v = 0; v < lodVerts * 4; ++v) r.readByte();
            for (std::uint32_t s = 0; s < lodSurfs; ++s) {
                r.readString();
                r.readULong();
                r.readByte();
                const auto mCount = r.readByte();
                for (std::uint8_t m = 0; m < mCount; ++m) r.readUShort();
                const auto fCount = r.readUInt();
                for (std::uint32_t f = 0; f < fCount * 3; ++f) r.readUInt();
            }
        }

        objectIndexByName[mesh.name] = objects.size();
        objects.push_back(std::move(mesh));
    }

    report("Reading " + std::to_string(staticInstanceCount) + " static instances...", 0.45f);

    // Read Static Instances
    r.seek(instanceOffset);
    std::vector<InstanceDef> instances;
    instances.reserve(staticInstanceCount + dynamicInstanceCount);
    for (std::uint32_t i = 0; i < staticInstanceCount; ++i) {
        InstanceDef inst;
        inst.name = r.readString();
        inst.position = {r.readFloat(), r.readFloat(), r.readFloat()};
        inst.rotationDegrees = {r.readFloat(), r.readFloat(), r.readFloat()};
        inst.scale = {r.readFloat(), r.readFloat(), r.readFloat()};
        instances.push_back(std::move(inst));
    }

    // Read Dynamic Instances
    for (std::uint32_t i = 0; i < dynamicInstanceCount; ++i) {
        InstanceDef inst;
        inst.name = r.readString();
        r.readString(); // DestroyedName
        r.readFloat();  // Health
        inst.position = {r.readFloat(), r.readFloat(), r.readFloat()};
        inst.rotationDegrees = {r.readFloat(), r.readFloat(), r.readFloat()};
        inst.scale = {r.readFloat(), r.readFloat(), r.readFloat()};
        r.readUInt();   // Type
        instances.push_back(std::move(inst));
    }

    const float unitScale = kInchesToCm * scaleMultiplier;
    const auto mapDir = path.parent_path();

    report("Scanning texture directories...", 0.55f);
    TextureIndex textureIndex;
    textureIndex.addDirectory(mapDir);
    textureIndex.addDirectory(mapDir / "_images");
    textureIndex.addDirectory(mapDir / "images");
    textureIndex.addDirectory(mapDir / "_images" / "mc");
    textureIndex.addDirectory(mapDir / "mc");

    auto cur = mapDir;
    for (int depth = 0; depth < 5; ++depth) {
        if (cur.empty() || !cur.has_parent_path()) break;
        cur = cur.parent_path();
        textureIndex.addDirectory(cur / "_images");
        textureIndex.addDirectory(cur / "images");
        textureIndex.addDirectory(cur / "exported_images");
        if (!gameName.empty()) {
            textureIndex.addDirectory(cur / "exported_images" / gameName);
            textureIndex.addDirectory(cur / "C2MBeta" / "exported_images" / gameName);
            textureIndex.addDirectory(cur / "C2MBeta 082026" / "C2MBeta" / "exported_images" / gameName);
            textureIndex.addDirectory(cur / "C2Mv3Beta" / "C2MBeta" / "exported_images" / gameName);
            textureIndex.addDirectory(cur / "C2M COD" / "C2M v2.2" / "exported_images" / gameName);
            textureIndex.addDirectory(cur / "C2M COD" / "C2Mv2LatestIW" / "exported_images" / gameName);
            textureIndex.addDirectory(cur / "C2M COD" / "C2Mv3" / "exported_images" / gameName);
        }
    }

    report("Instantiating world geometry & batching materials...", 0.60f);

    constexpr float kChunkSize = 12000.0f; // 120 meters in cm

    struct ChunkBatchKey {
        int cx{};
        int cy{};
        std::uint32_t matIdx{};

        bool operator==(const ChunkBatchKey& o) const noexcept {
            return cx == o.cx && cy == o.cy && matIdx == o.matIdx;
        }
    };

    struct ChunkBatchKeyHash {
        std::size_t operator()(const ChunkBatchKey& k) const noexcept {
            std::size_t h1 = std::hash<int>{}(k.cx);
            std::size_t h2 = std::hash<int>{}(k.cy);
            std::size_t h3 = std::hash<std::uint32_t>{}(k.matIdx);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    std::unordered_map<ChunkBatchKey, Mesh, ChunkBatchKeyHash> chunkBatches;
    std::vector<std::uint32_t> localRemap;
    std::vector<std::uint32_t> touchedIndices;

    const auto emitGeometry = [&](const MeshDef& meshDef, const Mat4& transform, bool isWorldBsp, const Vec3& instCenter, bool skipCollision) {
        const int instCx = static_cast<int>(std::floor(instCenter.x / kChunkSize));
        const int instCy = static_cast<int>(std::floor(instCenter.y / kChunkSize));

        for (const auto& surf : meshDef.surfaces) {
            if (surf.faces.empty()) continue;
            const std::uint32_t matIdx = surf.materials.empty() ? 0 : surf.materials[0];
            const auto* matDef = matIdx < materials.size() ? &materials[matIdx] : nullptr;
            const std::string matName = matDef ? matDef->name : "default";
            const std::string canonicalMat = canonical(matName);
            const bool skybox = extension.present
                ? (matIdx<extension.meta["visualMaterials"].size() && extension.meta["visualMaterials"][matIdx].value("sky",false))
                : (isSkyboxMaterial(canonicalMat, matDef ? canonical(matDef->techset) : "") || isSkyboxMaterial(canonical(surf.name), ""));
            if (skybox) continue;

            const bool toolInvisible = !extension.present&&(isToolOrInvisibleMaterial(canonicalMat) || isToolOrInvisibleMaterial(surf.name));
            const bool foliage = isAlphaTestedFoliage(canonicalMat);
            const bool isAdditive = !foliage && (isAdditiveSurface(canonicalMat, matDef ? canonical(matDef->techset) : ""));
            const bool isMultiplyTechset = matDef && (canonical(matDef->techset).find("multiply") != std::string::npos);
            const bool isDecalSort = matDef && (matDef->sortKey >= 9 && matDef->sortKey <= 16);
            const bool decal = !foliage && (isAdditive || isDecalSurface(canonicalMat) || isMultiplyTechset || isDecalSort);
            const bool decalAdditive = decal && isAdditive;
            const bool decalMultiply = decal && !decalAdditive && isMultiplyTechset;
            const bool transparent = !foliage && !decal && (matDef ? isAlphaTransparentMaterial(canonicalMat, canonical(matDef->techset), canonical(matDef->surfType)) : false);
            const bool isNonSolid = foliage || decal || transparent || isNonSolidFoliageOrDecal(canonicalMat) || isNonSolidFoliageOrDecal(canonical(surf.name));

            int cx = instCx;
            int cy = instCy;
            if (isWorldBsp) {
                Vec3 surfCenter{};
                std::size_t sampleCount = 0;
                for (const auto& face : surf.faces) {
                    for (int v = 0; v < 3; ++v) {
                        if (face[v] < meshDef.vertices.size()) {
                            surfCenter = surfCenter + meshDef.vertices[face[v]] * unitScale;
                            sampleCount++;
                        }
                    }
                    if (sampleCount >= 12) break;
                }
                if (sampleCount > 0) {
                    surfCenter = surfCenter * (1.0f / static_cast<float>(sampleCount));
                    cx = static_cast<int>(std::floor(surfCenter.x / kChunkSize));
                    cy = static_cast<int>(std::floor(surfCenter.y / kChunkSize));
                }
            }

            ChunkBatchKey key{cx, cy, matIdx};
            auto& targetMesh = chunkBatches[key];
            if (targetMesh.vertices.empty()) {
                targetMesh.name = "C2M / " + matName;
                targetMesh.materialName = matName;
                targetMesh.skinned = false;
                targetMesh.decal = decal;
                targetMesh.decalMultiply = decalMultiply;
                targetMesh.decalAdditive = decalAdditive;
                targetMesh.alphaTest = foliage;
                targetMesh.forceAlpha = transparent;
                targetMesh.gltfPbr = false;
                targetMesh.color = {1, 1, 1, 1};
                targetMesh.metallicFactor = 0.0f;
                targetMesh.roughnessFactor = 0.85f;
                if (matDef) {
                    targetMesh.albedoPath = resolveTextureFile(textureIndex, matDef->colorMap);
                    if (targetMesh.albedoPath.empty()) {
                        targetMesh.albedoPath = resolveTextureFile(textureIndex, matDef->name);
                    }
                    if (!matDef->normalMap.empty()) {
                        targetMesh.normalPath = resolveTextureFile(textureIndex, matDef->normalMap);
                    }
                    if (!matDef->specularMap.empty()) {
                        targetMesh.specularPath = resolveTextureFile(textureIndex, matDef->specularMap);
                    }
                }
                if(extension.present&&matIdx<extension.meta["visualMaterials"].size()){
                    const auto& explicitMaterial=extension.meta["visualMaterials"][matIdx];
                    if(explicitMaterial.contains("c2mMaterialName")&&explicitMaterial["c2mMaterialName"]!=matName)throw std::runtime_error("C2MX material index/name mismatch");
                    if(!codm::applyCodmMaterial(explicitMaterial,path.parent_path(),targetMesh,error))throw std::runtime_error(error);
                }
            }

            if (localRemap.size() < meshDef.vertices.size()) {
                localRemap.assign(meshDef.vertices.size(), UINT32_MAX);
            }

            for (const auto& face : surf.faces) {
                if (face[0] >= meshDef.vertices.size() || face[1] >= meshDef.vertices.size() || face[2] >= meshDef.vertices.size()) continue;

                std::array<std::uint32_t, 3> triangleIndices{};
                std::array<Vec3, 3> facePositions{};
                for (int v = 0; v < 3; ++v) {
                    const auto srcIdx = face[v];
                    std::uint32_t newIdx = localRemap[srcIdx];
                    if (newIdx == UINT32_MAX) {
                        newIdx = static_cast<std::uint32_t>(targetMesh.vertices.size());
                        localRemap[srcIdx] = newIdx;
                        touchedIndices.push_back(srcIdx);

                        Vertex vertex;
                        const auto rawPos = meshDef.vertices[srcIdx] * unitScale;
                        vertex.position = transformPoint(transform, rawPos);
                        if (srcIdx < meshDef.normals.size()) {
                            vertex.normal = normalize(transformPoint(transform, meshDef.normals[srcIdx]) - transformPoint(transform, Vec3{0, 0, 0}));
                        }
                        if (srcIdx < meshDef.uvs.size()) {
                            vertex.uv = meshDef.uvs[srcIdx];
                        }
                        if (srcIdx < meshDef.colors.size()) {
                            vertex.color = meshDef.colors[srcIdx];
                        }
                        if(extension.present&&targetMesh.vertexBlendBaked)vertex.color={1,1,1,1};
                        growBounds(map.scene.bounds, vertex.position);
                        growBounds(targetMesh.bounds, vertex.position);
                        if (!toolInvisible) {
                            targetMesh.vertices.push_back(vertex);
                        }
                    }
                    triangleIndices[v] = newIdx;
                    facePositions[v] = (srcIdx < meshDef.vertices.size()) ? transformPoint(transform, meshDef.vertices[srcIdx] * unitScale) : Vec3{};
                }

                if (!toolInvisible) {
                    targetMesh.indices.push_back(triangleIndices[0]);
                    targetMesh.indices.push_back(triangleIndices[1]);
                    targetMesh.indices.push_back(triangleIndices[2]);
                }

                // Collision geometry generation
                if (!extension.present && !toolInvisible && !skipCollision) {
                    const auto& v0 = facePositions[0];
                    const auto& v1 = facePositions[1];
                    const auto& v2 = facePositions[2];

                    scene::glb::CollisionTriangle tri;
                    tri.a = v0; tri.b = v1; tri.c = v2;
                    tri.normal = normalize(cross(tri.b - tri.a, tri.c - tri.a));
                    if (length(tri.normal) >= 0.5f) {
                        if (tri.normal.z < 0) {
                            std::swap(tri.b, tri.c);
                            tri.normal = tri.normal * -1.0f;
                        }
                        tri.minimum = {std::min({tri.a.x, tri.b.x, tri.c.x}), std::min({tri.a.y, tri.b.y, tri.c.y}), std::min({tri.a.z, tri.b.z, tri.c.z})};
                        tri.maximum = {std::max({tri.a.x, tri.b.x, tri.c.x}), std::max({tri.a.y, tri.b.y, tri.c.y}), std::max({tri.a.z, tri.b.z, tri.c.z})};
                        const float verticalSpan = tri.maximum.z - tri.minimum.z;
                        const float horizontalSpan = std::max(tri.maximum.x - tri.minimum.x, tri.maximum.y - tri.minimum.y);
                        // Walkable surfaces: floor/ramp/stairs (must NOT be non-solid foliage/decals/glass/water)
                        tri.walkable = !isNonSolid && tri.normal.z >= 0.50f && (horizontalSpan >= 10.0f || verticalSpan >= 6.0f);
                        // Blocking walls: substantial vertical barriers (at least 35cm high, 20cm wide)
                        tri.blocking = !isNonSolid && tri.normal.z < 0.35f && verticalSpan >= 35.0f && horizontalSpan >= 20.0f;
                        if (tri.walkable || tri.blocking) {
                            map.collision.push_back(tri);
                        }
                    }
                }
            }

            for (const auto touched : touchedIndices) {
                localRemap[touched] = UINT32_MAX;
            }
            touchedIndices.clear();
        }
    };

    // 1. Emit Object 0 (World BSP mapGeometry)
    if (!objects.empty()) {
        emitGeometry(objects[0], Mat4::identity(), true, Vec3{0, 0, 0}, false);
    }

    // 2. Emit Static and Dynamic Model Instances
    for (const auto& inst : instances) {
        const auto found = objectIndexByName.find(inst.name);
        if (found == objectIndexByName.end()) continue;
        const auto& meshDef = objects[found->second];

        const float pitchRad = inst.rotationDegrees.x * (kPi / 180.0f);
        const float yawRad = inst.rotationDegrees.y * (kPi / 180.0f);
        const float rollRad = inst.rotationDegrees.z * (kPi / 180.0f);

        const Mat4 instanceTransform = trs(inst.position * unitScale,
                                           fromEulerRadians({pitchRad, yawRad, rollRad}),
                                           inst.scale);

        const bool skipCollision = isDecorativePropModel(canonical(inst.name)) || isDecorativePropModel(canonical(meshDef.name));
        emitGeometry(meshDef, instanceTransform, false, inst.position * unitScale, skipCollision);
    }

    // Collect all material batches into scene meshes
    map.scene.meshes.reserve(chunkBatches.size());
    for (auto& [_, mesh] : chunkBatches) {
        if (!mesh.vertices.empty() && !mesh.indices.empty()) {
            map.scene.meshes.push_back(std::move(mesh));
        }
    }

    if (map.scene.meshes.empty()) {
        error = "C2M map contains no drawable geometry";
        return false;
    }

    report("Building spatial collision hash index (" + std::to_string(map.collision.size()) + " triangles)...", 0.85f);
    map.buildCollisionIndex();
    if(extension.present){
        // Authored collision owns spawn validation as well as physics. Never
        // accept an ungrounded legacy entity or visual-derived fallback here.
        c2mx::chooseAuthoredSpawn(map);
        report("Map import complete!",0.88f);return true;
    }

    // 3. Resolve Authored Player Spawn Point from Entity Definitions (entsOffset)
    bool foundAuthoredSpawn = false;
    if ((!extension.present||!map.collision.empty()) && entsOffset < extension.baseEnd) {
        const std::string entData(reinterpret_cast<const char*>(buffer.data() + entsOffset), extension.baseEnd - entsOffset);
        std::size_t pos = 0;
        std::vector<Vec3> primarySpawns;
        std::vector<Vec3> fallbackSpawns;

        while ((pos = entData.find('{', pos)) != std::string::npos) {
            const auto endPos = entData.find('}', pos);
            if (endPos == std::string::npos) break;
            const std::string block = entData.substr(pos, endPos - pos + 1);
            pos = endPos + 1;

            const auto originPos = block.find("\"origin\"");
            const auto classnamePos = block.find("\"classname\"");
            if (originPos == std::string::npos || classnamePos == std::string::npos) continue;

            std::string classname;
            const auto cnStart = block.find('"', classnamePos + 11);
            if (cnStart != std::string::npos) {
                const auto cnEnd = block.find('"', cnStart + 1);
                if (cnEnd != std::string::npos) {
                    classname = canonical(block.substr(cnStart + 1, cnEnd - cnStart - 1));
                }
            }

            float ox = 0, oy = 0, oz = 0;
            const auto origStart = block.find('"', originPos + 8);
            if (origStart != std::string::npos) {
                const auto origEnd = block.find('"', origStart + 1);
                if (origEnd != std::string::npos) {
                    const auto origStr = block.substr(origStart + 1, origEnd - origStart - 1);
                    std::sscanf(origStr.c_str(), "%f %f %f", &ox, &oy, &oz);
                }
            }

            const Vec3 spawnPos = Vec3{ox, oy, oz} * unitScale;
            if (classname == "mp_tdm_spawn" || classname == "mp_dm_spawn") {
                primarySpawns.insert(primarySpawns.begin(), spawnPos);
            } else if (classname.find("info_player_start") != std::string::npos ||
                       classname.find("mp_spawnpoint") != std::string::npos ||
                       classname.find("mp_global_intermission") != std::string::npos) {
                primarySpawns.push_back(spawnPos);
            } else if (classname.find("spawn") != std::string::npos ||
                       classname.find("weapon_") != std::string::npos) {
                fallbackSpawns.push_back(spawnPos);
            }
        }

        const auto& candidates = !primarySpawns.empty() ? primarySpawns : fallbackSpawns;
        if (!candidates.empty()) {
            // Pick the best candidate that snaps cleanly onto walkable collision ground
            for (const auto& candidate : candidates) {
                constexpr float noGround = -1e9f;
                const float ground = map.groundHeight(candidate.x, candidate.y, candidate.z + 150.0f, noGround);
                if (ground > noGround * 0.5f) {
                    map.defaultSpawnPoint = {candidate.x, candidate.y, ground + 5.0f};
                    map.hasDefaultSpawnPoint = true;
                    foundAuthoredSpawn = true;
                    break;
                }
            }
            if (!foundAuthoredSpawn) {
                map.defaultSpawnPoint = candidates.front();
                map.hasDefaultSpawnPoint = true;
                foundAuthoredSpawn = true;
            }
        }
    }

    // No entity spawn: seek supported ground near the collision population.
    if (!foundAuthoredSpawn) {
        if(const auto spawn=glb::findFallbackSpawn(map)){map.defaultSpawnPoint=*spawn;map.hasDefaultSpawnPoint=true;}
    }

    report("Map import complete!", 0.88f);
    return true;
}

bool load(const std::filesystem::path& path,glb::Map& map,std::string& error,float scaleMultiplier,
          const std::function<void(std::string_view,float)>& progress,const LoadOptions& options){
    try{glb::Map pending;if(!loadImpl(path,pending,error,scaleMultiplier,progress,options))return false;map=std::move(pending);return true;}
    catch(const std::exception& ex){error=std::string("C2M import: ")+ex.what();return false;}
}

} // namespace scene::c2m
