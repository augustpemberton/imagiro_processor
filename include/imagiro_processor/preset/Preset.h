// Preset.h
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <optional>
#include <filesystem>
#include <fstream>

#include "PresetMetadata.h"

namespace imagiro {

using json = nlohmann::json;

class Preset {
public:
    static constexpr int CurrentVersion = 2;

    Preset() = default;
    explicit Preset(PresetMetadata meta) : metadata_(std::move(meta)) {}

    PresetMetadata& metadata() { return metadata_; }
    const PresetMetadata& metadata() const { return metadata_; }

    json& state() { return state_; }
    const json& state() const { return state_; }

    json toJson() const {
        return json{
            {"version", CurrentVersion},
            {"metadata", metadata_},
            {"state", state_}
        };
    }

    static std::optional<Preset> fromJson(const json& j) {
        try {
            // Check version - legacy presets have no version or version < 2
            if (!j.contains("version") || j["version"].get<int>() < CurrentVersion) {
                return fromLegacySamplerJson(j);
            }

            Preset p;
            if (j.contains("metadata")) {
                p.metadata_ = j["metadata"].get<PresetMetadata>();
            }
            if (j.contains("state")) {
                p.state_ = j["state"];
            }
            return p;
        } catch (...) {
            return std::nullopt;
        }
    }

    static std::optional<Preset> fromLegacySamplerJson(const json& j) {
        try {
            Preset p;

            // Convert metadata
            p.metadata_.name = j.value("name", "");
            p.metadata_.description = j.value("description", "");

            // Convert paramStates array to params object
            // Old format: [["gain", 0.0, "default", 0.0], ...]
            // New format: {"gain": 0.0, ...}
            if (j.contains("paramStates") && j["paramStates"].is_array()) {
                json params = json::object();
                for (const auto& paramState : j["paramStates"]) {
                    if (paramState.is_array() && paramState.size() >= 2) {
                        std::string uid = paramState[0].get<std::string>();
                        params[uid] = paramState[1];
                    }
                }
                p.state_["params"] = params;
            }

            // Convert "data" to "samplerData"
            if (j.contains("data")) {
                p.state_["samplerData"] = j["data"];
            }

            return p;
        } catch (...) {
            return std::nullopt;
        }
    }

    bool saveToFile(const std::filesystem::path& path) const {
        std::ofstream file(path);
        if (!file) return false;
        file << toJson().dump(2);
        return file.good();
    }

    static std::optional<Preset> loadFromFile(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file) return std::nullopt;
        try {
            return fromJson(json::parse(file));
        } catch (...) {
            return std::nullopt;
        }
    }

    std::vector<uint8_t> toBinary() const {
        return json::to_msgpack(toJson());
    }

    static std::optional<Preset> fromBinary(const uint8_t* data, size_t size) {
        try {
            return fromJson(json::from_msgpack(data, data + size));
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    PresetMetadata metadata_;
    json state_;
};

} // namespace imagiro