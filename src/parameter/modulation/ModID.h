//
// Created by August Pemberton on 27/01/2025.
//

#pragma once

#include <functional>

namespace imagiro {
    typedef int SourceID;
    typedef int TargetID;
}

template<>
struct std::hash<std::pair<imagiro::SourceID, imagiro::TargetID>> {
    size_t operator()(const std::pair<imagiro::SourceID, imagiro::TargetID> &p) const noexcept {
        const size_t h1 = std::hash<imagiro::SourceID>{}(p.first);
        const size_t h2 = std::hash<imagiro::TargetID>{}(p.second);
        return h1 ^ (h2 << 1); // Simple hash combine
    }
};
