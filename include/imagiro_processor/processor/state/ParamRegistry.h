// ParamRegistry.h
#pragma once

#include "StateRegistry.h"
#include "imagiro_processor/parameter/ParamConfig.h"
#include "imagiro_processor/parameter/ParamState.h"

namespace imagiro {
    using ParamSnapshot = StateSnapshot<ParamState>;

    class ParamRegistry {
    public:
        Handle add(ParamConfig config) {
            Handle handle{static_cast<uint32_t>(configs_.size())};

            idToHandle_[config.uid] = handle;

            float defaultVal = config.range.clamp(config.defaultValue);
            float default01 = config.range.normalize(defaultVal);

            ParamState defaultState{
                .value01 = default01,
                .userValue = defaultVal,
                .processorValue = defaultVal
            };

            configs_.push_back(std::move(config));
            values_ = values_.set(handle.index, defaultState);

            return handle;
        }

        Handle handle(const std::string &uid) const {
            auto it = idToHandle_.find(uid);
            return it != idToHandle_.end() ? it->second : Handle::invalid();
        }

        bool has(const std::string &uid) const {
            return idToHandle_.contains(uid);
        }

        const ParamConfig &config(Handle h) const {
            return configs_[h.index];
        }

        size_t size() const { return configs_.size(); }

        void setValue(Handle h, float userValue) {
            const auto &cfg = configs_[h.index];
            float clamped = cfg.range.clamp(userValue);
            float normalized = cfg.range.normalize(clamped);
            setValue01(h, normalized);
        }

        void setValue01(Handle h, float normalized) {
            const auto &cfg = configs_[h.index];

            float clamped = std::clamp(normalized, 0.f, 1.f);
            float userValue = cfg.range.denormalize(clamped);

            ParamState state{
                .value01 = clamped,
                .userValue = userValue,
                .processorValue = userValue
            };

            values_ = values_.set(h.index, state);
        }

        void resetToDefault(Handle h) {
            setValue(h, configs_[h.index].defaultValue);
        }

        float getValue(Handle h) const {
            return values_.find(h.index)->userValue;
        }

        float getValue01(Handle h) const {
            return values_.find(h.index)->value01;
        }

        std::string getValueText(Handle h) const {
            return configs_[h.index].format.toString(getValue(h));
        }

        bool setValueFromText(Handle h, const std::string &text) {
            if (auto parsed = configs_[h.index].format.fromString(text)) {
                setValue(h, *parsed);
                return true;
            }
            return false;
        }

        ParamSnapshot capture(double bpm, double sampleRate) const {
            immer::map<uint32_t, ParamState> result;

            for (size_t i = 0; i < configs_.size(); i++) {
                const auto &cfg = configs_[i];
                const auto *current = values_.find(static_cast<uint32_t>(i));

                float processorValue = cfg.toProcessor
                                           ? cfg.toProcessor(current->userValue, bpm, sampleRate)
                                           : current->userValue;

                result = result.set(static_cast<uint32_t>(i), ParamState{
                                        .value01 = current->value01,
                                        .userValue = current->userValue,
                                        .processorValue = processorValue
                                    });
            }

            return ParamSnapshot(std::move(result));
        }

        json toJson() const {
            json j = json::object();
            for (size_t i = 0; i < configs_.size(); i++) {
                const auto *state = values_.find(static_cast<uint32_t>(i));
                j[configs_[i].uid] = state->value01;
            }
            return j;
        }

        void fromJson(const json &j) {
            for (size_t i = 0; i < configs_.size(); i++) {
                const auto &uid = configs_[i].uid;
                if (j.contains(uid)) {
                    setValue01(Handle{static_cast<uint32_t>(i)}, j[uid].get<float>());
                }
            }
        }

        template<typename Func>
        void forEach(Func &&fn) const {
            for (size_t i = 0; i < configs_.size(); i++) {
                fn(Handle{static_cast<uint32_t>(i)}, configs_[i]);
            }
        }

    private:
        std::deque<ParamConfig> configs_;
        immer::map<uint32_t, ParamState> values_;
        std::unordered_map<std::string, Handle> idToHandle_;
    };
} // namespace imagiro
