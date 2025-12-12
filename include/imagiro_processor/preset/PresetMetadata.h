//
// Created by August Pemberton on 12/12/2025.
//

#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace imagiro {
    using json = nlohmann::json;


    struct PresetMetadata {
        std::string name;
        std::string author;
        std::string description;
        int version{1};
    };

    inline void to_json(json &j, const PresetMetadata &m) {
        j = json{
            {"name", m.name},
            {"author", m.author},
            {"description", m.description},
            {"version", m.version}
        };
    }

    inline void from_json(const json &j, PresetMetadata &m) {
        m.name = j.value("name", "");
        m.author = j.value("author", "");
        m.description = j.value("description", "");
        m.version = j.value("version", 1);
    }
}
