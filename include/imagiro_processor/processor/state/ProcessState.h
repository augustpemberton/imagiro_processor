// ProcessState.h
#pragma once

#include "StateRegistry.h"
#include <unordered_map>

#include "immer/box.hpp"

namespace imagiro {

    class ProcessState {
    public:
        template<typename T>
        void add(const std::string& id, const StateRegistry<T>& registry) {
            registries_ = registries_.set(id, AnyRegistry(registry));
        }

        template<typename T>
        const StateRegistry<T>& get(const std::string& id) const {
            return registries_.at(id).as<T>();
        }

        bool has(const std::string& id) const {
            return registries_.find(id) != nullptr;
        }

        immer::box<StateRegistry<ParamValue>>& params() { return params_; }

        float value01(Handle h) const { return params_->get(h).value01; }
        float userValue(Handle h) const { return params_->get(h).userValue; }

        float value(Handle h) const {
            const auto& pv = params_->get(h);
            return pv.toProcessor
                ? pv.toProcessor(pv.userValue, bpm_, sampleRate_)
                : pv.userValue;
        }

        json toJson() const {
            json j = json::object();
            for (const auto& [id, reg] : registries_) {
                j[id] = reg;
            }
            return j;
        }

        double bpm() const { return bpm_; }
        double sampleRate() const { return sampleRate_; }

        void setBpm(double bpm) { bpm_ = bpm; }
        void setSampleRate(double sr) { sampleRate_ = sr; }

    private:
        immer::box<StateRegistry<ParamValue>> params_;
        immer::map<std::string, AnyRegistry> registries_;
        double bpm_{120.0};
        double sampleRate_{44100.0};
    };

} // namespace imagiro