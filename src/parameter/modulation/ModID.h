//
// Created by August Pemberton on 27/01/2025.
//

#pragma once

namespace imagiro {
    struct SourceID {
        uint value;
        bool operator==(const SourceID& other) const {
            return value == other.value;
        }
    };

    struct TargetID {
        uint value;
        bool operator==(const TargetID& other) const {
            return value == other.value;
        }
    };
}

namespace std {
    template<>
    struct hash<imagiro::SourceID> {
        size_t operator()(const imagiro::SourceID &p) const {
            return std::hash<uint>{}(p.value);
        }
    };

    template<>
    struct hash<imagiro::TargetID> {
        size_t operator()(const imagiro::TargetID&p) const {
            return std::hash<uint>{}(p.value);
        }
    };

    template<>
    struct hash<std::pair<imagiro::SourceID, imagiro::TargetID>> {
        size_t operator()(const std::pair<imagiro::SourceID, imagiro::TargetID> &p) const {
            size_t h1 = std::hash<imagiro::SourceID>{}(p.first);
            size_t h2 = std::hash<imagiro::TargetID>{}(p.second);
            return h1 ^ (h2 << 1); // Simple hash combine
        }
    };
}
