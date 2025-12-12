// ParamController.h
#pragma once

#include "ParamValue.h"
#include "ParamConfig.h"
#include <sigslot/sigslot.h>
#include <deque>
#include <atomic>

#include "imagiro_processor/processor/state/StateRegistry.h"

namespace imagiro {

class ParamController {
public:
    explicit ParamController(StateRegistry<ParamValue>& registry)
        : registry_(registry) {}

    Handle add(ParamConfig config) {
        const auto defaultVal = config.range.clamp(config.defaultValue);
        const auto default01 = config.range.normalize(defaultVal);

        const ParamValue value{
            .value01 = default01,
            .userValue = defaultVal,
            .toProcessor = config.toProcessor  // copy the fn pointer
        };

        // Use mutable add during setup
        const Handle h = registry_.add(config.uid, value);

        configs_.push_back(std::move(config));
        uiSignals_.emplace_back();
        audioSignals_.emplace_back();
        dirtyUI_.emplace_back(false);
        dirtyAudio_.emplace_back(false);

        return h;
    }

    Handle handle(const std::string& uid) const {
        return registry_.handle(uid);
    }

    bool has(const std::string& uid) const {
        return registry_.has(uid);
    }

    const ParamConfig& config(Handle h) const {
        return configs_[h.index];
    }

    size_t size() const { return configs_.size(); }

    // --- Value setters (mutate registry in place during UI interaction) ---

    void setValue(Handle h, float userValue) {
        const auto& cfg = configs_[h.index];
        float clamped = cfg.range.clamp(userValue);
        float normalized = cfg.range.normalize(clamped);
        setValue01(h, normalized);
    }

    void setValue01(Handle h, float normalized) {
        if (almostEqual(registry_.get(h).value01, normalized)) return;

        const auto& cfg = configs_[h.index];
        const float clamped = std::clamp(normalized, 0.f, 1.f);
        const float userValue = cfg.range.denormalize(clamped);

        const ParamValue value{
            .value01 = clamped,
            .userValue = userValue,
            .toProcessor = cfg.toProcessor  // copy the fn pointer
        };

        registry_ = registry_.set(h, value);

        dirtyUI_[h.index].store(true, std::memory_order_relaxed);
        dirtyAudio_[h.index].store(true, std::memory_order_relaxed);
    }

    void resetToDefault(Handle h) {
        setValue(h, configs_[h.index].defaultValue);
    }

    // --- Value getters ---

    float getValue(Handle h) const {
        return registry_.get(h).userValue;
    }

    float getValue01(Handle h) const {
        return registry_.get(h).value01;
    }

    std::string getValueText(Handle h) const {
        return configs_[h.index].format.toString(getValue(h));
    }

    bool setValueFromText(Handle h, const std::string& text) {
        if (const auto parsed = configs_[h.index].format.fromString(text)) {
            setValue(h, *parsed);
            return true;
        }
        return false;
    }

    // --- Signals ---

    sigslot::signal<float>& uiSignal(Handle h) {
        return uiSignals_[h.index];
    }

    sigslot::signal<float>& audioSignal(Handle h) {
        return audioSignals_[h.index];
    }

    // Call from message thread timer
    void dispatchUIChanges() {
        for (size_t i = 0; i < configs_.size(); i++) {
            if (dirtyUI_[i].exchange(false, std::memory_order_relaxed)) {
                float userValue = registry_.get(Handle{static_cast<uint32_t>(i)}).userValue;
                uiSignals_[i](userValue);
            }
        }
    }

    // --- Capture for audio thread ---

    // Returns a new registry with processorValues computed, dispatches audio signals
    StateRegistry<ParamValue> capture() {
        for (size_t i = 0; i < configs_.size(); i++) {
            if (dirtyAudio_[i].exchange(false, std::memory_order_relaxed)) {
                const auto& pv = registry_.get(Handle{static_cast<uint32_t>(i)});
                audioSignals_[i](pv.userValue);
            }
        }
        return registry_;
    }

    // After loading from JSON, recompute userValue from value01
    void recomputeFromJson() {
        for (size_t i = 0; i < configs_.size(); i++) {
            const Handle h{static_cast<uint32_t>(i)};
            const auto& cfg = configs_[i];
            ParamValue value = registry_.get(h);

            value.userValue = cfg.range.denormalize(value.value01);
            value.toProcessor = cfg.toProcessor;

            registry_ = registry_.set(h, value);
            dirtyUI_[i].store(true, std::memory_order_relaxed);
            dirtyAudio_[i].store(true, std::memory_order_relaxed);
        }
    }

    template<typename Func>
    void forEach(Func&& fn) const {
        for (size_t i = 0; i < configs_.size(); i++) {
            fn(Handle{static_cast<uint32_t>(i)}, configs_[i]);
        }
    }

    // Access underlying registry (for serialization)
    const StateRegistry<ParamValue>& registry() const { return registry_; }

private:
    StateRegistry<ParamValue>& registry_;
    std::deque<ParamConfig> configs_;

    std::deque<sigslot::signal<float>> uiSignals_;
    std::deque<sigslot::signal<float>> audioSignals_;

    std::deque<std::atomic<bool>> dirtyUI_;
    std::deque<std::atomic<bool>> dirtyAudio_;
};

} // namespace imagiro