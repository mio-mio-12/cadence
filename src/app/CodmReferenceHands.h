#pragma once
#include "assets/AssetCatalog.h"
#include <algorithm>
#include <cctype>
#include <optional>
namespace cadence::codm {
inline std::optional<std::size_t> referenceHands(const assets::Catalog& catalog) {
    const auto lower=[](std::string s) {
        std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        return s;
    };
    // Prefer the verified current Ghost identity, retaining the older alias.
    // Do not substitute an arbitrary hand model or a different Ghost skin.
    for(const char* name:{"codm_viewhands_c_m_ghost_1p","viewhands_c_m_ghost_1p",
                         "codm_viewhands_c_m_ghost_default"})
        for(std::size_t i=0;i<catalog.entries.size();++i) {
            const auto& a=catalog.entries[i];
            if(a.role==assets::Role::ViewHands && lower(a.game)=="codm" && lower(a.name)==name)
                return i;
        }
    return std::nullopt;
}
}
