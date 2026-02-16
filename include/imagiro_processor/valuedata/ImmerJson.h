#pragma once
#include <nlohmann/json.hpp>
#include <immer/vector.hpp>
#include <immer/flex_vector.hpp>

namespace nlohmann {
    template<typename T>
    struct adl_serializer<immer::vector<T>> {
        static void to_json(json& j, const immer::vector<T>& v) {
            j = json::array();
            for (const auto& item : v) j.push_back(item);
        }

        static void from_json(const json& j, immer::vector<T>& v) {
            v = {};
            for (const auto& item : j) {
                v = v.push_back(item.template get<T>());
            }
        }
    };

    template<typename T>
    struct adl_serializer<immer::flex_vector<T>> {
        static void to_json(json& j, const immer::flex_vector<T>& v) {
            j = json::array();
            for (const auto& item : v) j.push_back(item);
        }

        static void from_json(const json& j, immer::flex_vector<T>& v) {
            v = {};
            for (const auto& item : j) {
                v = v.push_back(item.template get<T>());
            }
        }
    };
}
