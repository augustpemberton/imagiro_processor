// ParamController.h
#pragma once

#include "ParamValue.h"
#include "ParamConfig.h"
#include <sigslot/sigslot.h>
#include <deque>
#include <atomic>
#include <memory>
#include <imagiro_util/readerwriterqueue/concurrentqueue.h>

#include "imagiro_processor/processor/state/StateRegistry.h"

namespace imagiro {

class ParamController {
public:
    ParamController()
        : registry_(std::make_shared<StateRegistry<ParamValue>>()) {}

    ParamController(const ParamController&) = delete;
    ParamController& operator=(const ParamController&) = delete;

    // =====================================================================
    // Setup (call before processing starts)
    // =====================================================================

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
        uiSignals_.emplace_back();
        audioSignals_.emplace_back();

        return h;
    }

    // =====================================================================
    // Thread-safe
    // =====================================================================

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

    void setValue(Handle h, float userValue) {
        const auto& cfg = configs_[h.index];
        setValue01(h, cfg.range.normalize(cfg.range.clamp(userValue)));
    }

    void setValue01(Handle h, float normalized) {
        pendingChanges_.enqueue({h, std::clamp(normalized, 0.f, 1.f)});
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

    // =====================================================================
    // UI thread
    // =====================================================================

    float getValueUI(Handle h) const {
        return std::atomic_load(&registry_)->get(h).userValue;
    }

    float getValue01UI(Handle h) const {
        return std::atomic_load(&registry_)->get(h).value01;
    }

    std::string getValueTextUI(Handle h) const {
        return configs_[h.index].format.toString(getValueUI(h));
    }

    bool setValueFromTextUI(Handle h, const std::string& text) {
        if (const auto parsed = configs_[h.index].format.fromString(text)) {
            setValue(h, *parsed);
            return true;
        }
        return false;
    }

    void dispatchUIChanges() {
        auto snapshot = std::atomic_load(&registry_);

        for (size_t i = 0; i < configs_.size(); i++) {
            if (uiDirty_[i].exchange(false, std::memory_order_acq_rel)) {
                uiSignals_[i](snapshot->get(Handle{static_cast<uint32_t>(i)}).userValue);
            }
        }
    }

    StateRegistry<ParamValue> registryUI() const {
        return *std::atomic_load(&registry_);
    }

    void setRegistryUI(StateRegistry<ParamValue> reg) {
        for (size_t i = 0; i < configs_.size(); i++) {
            const Handle h{static_cast<uint32_t>(i)};
            const auto& cfg = configs_[i];

            if (auto* pv = reg.tryGet(h)) {
                ParamValue value = *pv;
                value.userValue = cfg.range.denormalize(value.value01);
                value.toProcessor = cfg.toProcessor;
                reg = reg.set(h, value);
            }

            uiDirty_[i].store(true, std::memory_order_release);
        }

        std::atomic_store(&registry_,
            std::make_shared<StateRegistry<ParamValue>>(std::move(reg)));
    }

    // =====================================================================
    // Audio thread
    // =====================================================================

    StateRegistry<ParamValue> captureAudio() {
        auto current = std::atomic_load(&registry_);

        ParamChange change;
        bool changed = false;

        while (pendingChanges_.try_dequeue(change)) {
            const auto& cfg = configs_[change.handle.index];

            const auto* existing = current->tryGet(change.handle);
            if (existing && almostEqual(existing->value01, change.normalized)) {
                continue;
            }

            const ParamValue value{
                .value01 = change.normalized,
                .userValue = cfg.range.denormalize(change.normalized),
                .toProcessor = cfg.toProcessor
            };

            current = std::make_shared<StateRegistry<ParamValue>>(
                current->set(change.handle, value));
            uiDirty_[change.handle.index].store(true, std::memory_order_release);
            changed = true;
        }

        if (changed) {
            std::atomic_store(&registry_, current);
        }

        return *current;
    }

private:
    struct ParamChange {
        Handle handle;
        float normalized;
    };

    std::shared_ptr<StateRegistry<ParamValue>> registry_;
    moodycamel::ConcurrentQueue<ParamChange> pendingChanges_{64};

    std::deque<ParamConfig> configs_;
    std::deque<std::atomic<bool>> uiDirty_;
    std::deque<sigslot::signal<float>> uiSignals_;
    std::deque<sigslot::signal<float>> audioSignals_;
};

} // namespace imagiro