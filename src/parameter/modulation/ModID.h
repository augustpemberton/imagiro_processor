//
// Created by August Pemberton on 27/01/2025.
//

#pragma once

namespace imagiro {

    struct SourceID {
        uint value {0};
        uint voiceID {0}; // 0 = global

        bool operator==(const SourceID& other) const {
            return value == other.value && voiceID == other.voiceID;
        }
    };

    struct TargetID {
        uint value {0};
        uint voiceID {0}; // 0 = global

        bool operator==(const TargetID& other) const {
            return value == other.value && voiceID == other.voiceID;
        }
    };
}

namespace std {
    template<>
    struct hash<imagiro::SourceID> {
        size_t operator()(const imagiro::SourceID &p) const {
            auto h1 = std::hash<uint>{}(p.value);
            auto h2 = std::hash<uint>{}(p.voiceID);
            return h1 ^ (h2 << 1); // Simple hash combine
        }
    };

    template<>
    struct hash<imagiro::TargetID> {
        size_t operator()(const imagiro::TargetID&p) const {
            auto h1 = std::hash<uint>{}(p.value);
            auto h2 = std::hash<uint>{}(p.voiceID);
            return h1 ^ (h2 << 1); // Simple hash combine
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
