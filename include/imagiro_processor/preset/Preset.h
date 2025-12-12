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
    Preset() = default;
    explicit Preset(PresetMetadata meta) : metadata_(std::move(meta)) {}

    PresetMetadata& metadata() { return metadata_; }
    const PresetMetadata& metadata() const { return metadata_; }

    json& state() { return state_; }
    const json& state() const { return state_; }

    json toJson() const {
        return json{
            {"metadata", metadata_},
            {"state", state_}
        };
    }

    static std::optional<Preset> fromJson(const json& j) {
        try {
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