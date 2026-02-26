// ParamController.h
#pragma once

#include "ParamValue.h"
#include "ParamConfig.h"
#include <sigslot/sigslot.h>
#include <deque>
#include <atomic>
#include <memory>
#include <cmath>
#include <imagiro_util/util.h>

#include "imagiro_processor/processor/state/StateRegistry.h"

namespace imagiro {

class ParamController {
public:
    ParamController()
        : registry_(std::make_shared<StateRegistry<ParamValue>>()) {}

    ParamController(const ParamController&) = delete;
    ParamController& operator=(const ParamController&) = delete;

    Handle addParam(ParamConfig config) {
        const auto defaultVal = config.range.clamp(config.defaultValue);
        const auto default01 = config.range.normalize(defaultVal);

        const ParamValue value{
            .value01 = default01,
            .userValue = defaultVal,
            .toProcessor = config.toProcessor
        };

        auto current = std::atomic_load(&registry_);
        auto updated = std::make_shared<StateRegistry<ParamValue>>(*current);
        const Handle h = updated->add(config.uid, value);
        std::atomic_store(&registry_, updated);

        configs_.push_back(std::move(config));
        uiDirty_.emplace_back(false);
        audioDirty_.emplace_back(false);
        locked_.emplace_back(false);
        values01_.emplace_back(default01);
        uiSignals_.emplace_back();
        audioSignals_.emplace_back();

        return h;
    }

    Handle handle(const std::string& uid) const {
        return std::atomic_load(&registry_)->handle(uid);
    }

    bool has(const std::string& uid) const {
        return std::atomic_load(&registry_)->has(uid);
    }

    const ParamConfig& config(Handle h) const {
        return configs_[h.index];
    }

    size_t size() const { return configs_.size(); }

    void setLocked(Handle h, bool locked) { locked_[h.index].store(locked, std::memory_order_release); }
    bool isLocked(Handle h) const { return locked_[h.index].load(std::memory_order_acquire); }

    void setValue(Handle h, float userValue) {
        const auto& cfg = configs_[h.index];
        setValue01(h, cfg.range.normalize(cfg.range.clamp(userValue)));
    }

    void setValue01(Handle h, float normalized) {
        auto clamped = std::clamp(normalized, 0.f, 1.f);
        values01_[h.index].store(clamped, std::memory_order_release);
        uiDirty_[h.index].store(true, std::memory_order_release);
        audioDirty_[h.index].store(true, std::memory_order_release);
    }

    void resetToDefault(Handle h) {
        setValue(h, configs_[h.index].defaultValue);
    }

    const ValueFormatter& getFormatter(Handle h) const {
        return configs_[h.index].format;
    }

    sigslot::signal<float>& uiSignal(Handle h) {
        return uiSignals_[h.index];
    }

    sigslot::signal<float>& audioSignal(Handle h) {
        return audioSignals_[h.index];
    }

    template<typename Func>
    void forEach(Func&& fn) const {
        for (size_t i = 0; i < configs_.size(); i++) {
            fn(Handle{static_cast<uint32_t>(i)}, configs_[i]);
        }
    }

    float getValue(Handle h) const {
        auto v01 = values01_[h.index].load(std::memory_order_acquire);
        return configs_[h.index].range.denormalize(v01);
    }

    float getValue01(Handle h) const {
        return values01_[h.index].load(std::memory_order_acquire);
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

    void dispatchUIChanges() {
        for (size_t i = 0; i < configs_.size(); i++) {
            if (uiDirty_[i].exchange(false, std::memory_order_acq_rel)) {
                auto v01 = values01_[i].load(std::memory_order_acquire);
                auto userVal = configs_[i].range.denormalize(v01);
                uiSignals_[i](userVal);

                // Sync to registry for preset serialization
                const Handle h{static_cast<uint32_t>(i)};
                auto current = std::atomic_load(&registry_);
                const ParamValue pv{
                    .value01 = v01,
                    .userValue = userVal,
                    .toProcessor = configs_[i].toProcessor
                };
                auto updated = std::make_shared<StateRegistry<ParamValue>>(
                    current->set(h, pv));
                std::atomic_store(&registry_, updated);
            }
        }
    }

    StateRegistry<ParamValue> registryUI() const {
        return *std::atomic_load(&registry_);
    }

    void setRegistryUI(StateRegistry<ParamValue> reg, bool skipLocked = true) {
        for (size_t i = 0; i < configs_.size(); i++) {
            const Handle h{static_cast<uint32_t>(i)};
            const auto& cfg = configs_[i];

            if (skipLocked && locked_[i].load(std::memory_order_acquire)) {
                // Keep current value in the registry so serialization stays correct
                auto v01 = values01_[i].load(std::memory_order_acquire);
                ParamValue current{
                    .value01 = v01,
                    .userValue = cfg.range.denormalize(v01),
                    .toProcessor = cfg.toProcessor
                };
                reg = reg.set(h, current);
                continue;
            }

            if (auto* pv = reg.tryGet(h)) {
                ParamValue value = *pv;
                value.userValue = cfg.range.clamp(value.userValue);
                value.value01 = cfg.range.normalize(value.userValue);
                value.toProcessor = cfg.toProcessor;
                reg = reg.set(h, value);

                values01_[i].store(value.value01, std::memory_order_release);
            }

            uiDirty_[i].store(true, std::memory_order_release);
            audioDirty_[i].store(true, std::memory_order_release);
        }

        std::atomic_store(&registry_,
            std::make_shared<StateRegistry<ParamValue>>(std::move(reg)));
    }

    // =====================================================================
    // Audio thread
    // =====================================================================

    void dispatchAudioChanges() {
        for (size_t i = 0; i < configs_.size(); i++) {
            if (audioDirty_[i].exchange(false, std::memory_order_acq_rel)) {
                auto v01 = values01_[i].load(std::memory_order_acquire);
                auto userVal = configs_[i].range.denormalize(v01);
                audioSignals_[i](userVal);
            }
        }
    }

    void snapshotInto(std::vector<ParamValue>& out) {
        for (size_t i = 0; i < configs_.size(); i++) {
            auto v01 = values01_[i].load(std::memory_order_acquire);
            out[i].value01 = v01;
            out[i].userValue = configs_[i].range.denormalize(v01);
            out[i].toProcessor = configs_[i].toProcessor;
        }
    }

private:
    std::shared_ptr<StateRegistry<ParamValue>> registry_;

    std::deque<ParamConfig> configs_;
    std::deque<std::atomic<bool>> uiDirty_;
    std::deque<std::atomic<bool>> audioDirty_;
    std::deque<std::atomic<bool>> locked_;
    std::deque<std::atomic<float>> values01_;
    std::deque<sigslot::signal<float>> uiSignals_;
    std::deque<sigslot::signal<float>> audioSignals_;
};

} // namespace imagiro
