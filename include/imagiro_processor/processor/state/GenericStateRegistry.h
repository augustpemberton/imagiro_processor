//
// Created by August Pemberton on 12/12/2025.
//

#pragma once
#include "StateRegistry.h"

namespace imagiro {
    using GenericStateTag = StateHandle<std::any>;

    // Specialization for std::any - needs type-erased JSON conversion
    template<>
    class StateRegistry<std::any, GenericStateTag> {
    public:
        using StateHandleType = StateHandle<GenericStateTag>;
        using SnapshotType = StateSnapshot<std::any, GenericStateTag>;

        template<typename U>
        StateHandleType add(std::string id, U defaultValue, bool saveInPreset = true) {
            const StateHandleType handle{static_cast<uint32_t>(configs_.size())};

            idToStateHandle_[id] = handle;

            configs_.push_back(GenericStateConfig{
                .id = std::move(id),
                .defaultValue = defaultValue,
                .saveInPreset = saveInPreset,
                .toJson = [](const std::any &v) -> json {
                    return std::any_cast<const U &>(v);
                },
                .fromJson = [](const json &j) -> std::any {
                    return j.get<U>();
                }
            });

            values_ = values_.set(handle.index, std::any(std::move(defaultValue)));

            return handle;
        }

        StateHandleType handle(const std::string &id) const {
            const auto it = idToStateHandle_.find(id);
            return it != idToStateHandle_.end() ? it->second : StateHandleType::invalid();
        }

        bool has(const std::string &id) const {
            return idToStateHandle_.contains(id);
        }

        template<typename U>
        void set(StateHandleType h, U value) {
            values_ = values_.set(h.index, std::any(std::move(value)));
        }

        template<typename U>
        const U &get(StateHandleType h) const {
            return std::any_cast<const U &>(*values_.find(h.index));
        }

        SnapshotType capture() const {
            return SnapshotType(values_);
        }

        json toJson(bool onlyPresetData = true) const {
            json j = json::object();

            for (size_t i = 0; i < configs_.size(); i++) {
                const auto &config = configs_[i];
                if (onlyPresetData && !config.saveInPreset) continue;

                if (config.toJson) {
                    if (const auto *value = values_.find(static_cast<uint32_t>(i))) {
                        j[config.id] = config.toJson(*value);
                    }
                }
            }

            return j;
        }

        void fromJson(const json &j) {
            for (size_t i = 0; i < configs_.size(); i++) {
                const auto &config = configs_[i];

                if (j.contains(config.id) && config.fromJson) {
                    try {
                        values_ = values_.set(
                            static_cast<uint32_t>(i),
                            config.fromJson(j[config.id])
                        );
                    } catch (...) {
                    }
                }
            }
        }

        void resetToDefaults() {
            for (size_t i = 0; i < configs_.size(); i++) {
                values_ = values_.set(
                    static_cast<uint32_t>(i),
                    configs_[i].defaultValue
                );
            }
        }

        size_t size() const { return configs_.size(); }

    private:
        struct GenericStateConfig {
            std::string id;
            std::any defaultValue;
            bool saveInPreset{true};

            std::function<json(const std::any &)> toJson;
            std::function<std::any(const json &)> fromJson;
        };

        std::deque<GenericStateConfig> configs_;
        immer::map<uint32_t, std::any> values_;
        std::unordered_map<std::string, StateHandleType> idToStateHandle_;
    };


    // Convenience
    inline void to_json(json &j, const std::filesystem::path &p) { j = p.string(); }
    inline void from_json(const json &j, std::filesystem::path &p) { p = std::filesystem::path(j.get<std::string>()); }
}