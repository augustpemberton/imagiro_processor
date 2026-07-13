#pragma once

#include <clap/clap.h>

#include "../parameter/HostParamBridge.h"
#include "../parameter/ParamController.h"
#include <imagiro_util/readerwriterqueue/concurrentqueue.h>

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string_view>
#include <vector>

namespace imagiro {

// Frozen FNV-1a (32-bit) over a param uid -> stable CLAP param id. Duplicated
// here (also in ClapProcessor) so the bridge is self-contained; the mapping is
// frozen and must match.
constexpr uint32_t clapBridgeParamId(std::string_view uid) {
    uint32_t h = 2166136261u;
    for (char c : uid) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }
    return h;
}

// JUCE-free HostParamBridge for the CLAP shell. UI-thread gestures/edits are
// enqueued lock-free and drained into the host's output event stream on the
// audio thread (during process() and paramsFlush()). Mirrors JuceParamAdapter:
// value pushes are driven off ParamController::uiSignal (fired by
// dispatchUIChanges on the UI thread), and gesture wrapping forces a
// dispatch so the final value is ordered inside its gesture.
class ClapParamBridge : public HostParamBridge {
public:
    explicit ClapParamBridge(ParamController& params)
        : params_(params) {
        const auto n = params_.size();
        ids_.reserve(n);
        lastSent_.assign(n, std::numeric_limits<double>::quiet_NaN());

        params_.forEach([&](Handle h, const ParamConfig& cfg) {
            ids_.push_back(clapBridgeParamId(cfg.uid));
            conns_.push_back(params_.uiSignal(h).connect_scoped(
                [this, h](float) { onUiChanged(h); }));
        });
    }

    // Host proxy hook: ClapProcessor sets this to request a param flush so UI
    // edits reach the host even when the audio engine is idle.
    void setRequestFlush(std::function<void()> fn) { requestFlush_ = std::move(fn); }

    void beginGesture(Handle h) override {
        params_.dispatchUIChanges();               // order any pending edit before begin
        enqueue({ids_[h.index], EventKind::GestureBegin, 0.0});
        requestFlushIfPossible();
    }

    void endGesture(Handle h) override {
        params_.dispatchUIChanges();               // push the final in-gesture value first
        enqueue({ids_[h.index], EventKind::GestureEnd, 0.0});
        requestFlushIfPossible();
    }

    void pushValueToHost(Handle h, float value01) override {
        const double denorm = params_.config(h).range.denormalize(value01);
        enqueue({ids_[h.index], EventKind::Value, denorm});
        lastSent_[h.index] = denorm;
        requestFlushIfPossible();
    }

    // Called by ClapProcessor when the host itself sets a param, so the ensuing
    // uiSignal echo is suppressed (no feedback loop back to the host).
    void noteHostValue(Handle h, double denorm) {
        if (h.index < lastSent_.size()) lastSent_[h.index] = denorm;
    }

    // Audio-thread: drain queued events into the host output stream.
    void flush(const clap_output_events* out) {
        if (!out) return;
        Event e;
        while (queue_.try_dequeue(e)) {
            if (e.kind == EventKind::Value) {
                clap_event_param_value ev{};
                ev.header.size = sizeof(ev);
                ev.header.type = CLAP_EVENT_PARAM_VALUE;
                ev.header.time = 0;
                ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                ev.header.flags = 0;
                ev.param_id = e.id;
                ev.cookie = nullptr;
                ev.note_id = -1;
                ev.port_index = -1;
                ev.channel = -1;
                ev.key = -1;
                ev.value = e.value;
                out->try_push(out, &ev.header);
            } else {
                clap_event_param_gesture ev{};
                ev.header.size = sizeof(ev);
                ev.header.type = (e.kind == EventKind::GestureBegin)
                                     ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                     : CLAP_EVENT_PARAM_GESTURE_END;
                ev.header.time = 0;
                ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                ev.header.flags = 0;
                ev.param_id = e.id;
                out->try_push(out, &ev.header);
            }
        }
    }

private:
    enum class EventKind { Value, GestureBegin, GestureEnd };
    struct Event {
        clap_id id;
        EventKind kind;
        double value;
    };

    void onUiChanged(Handle h) {
        const double denorm = params_.getValue(h);
        const double last = lastSent_[h.index];
        if (!std::isnan(last) && std::abs(denorm - last) < 1e-9) return;
        enqueue({ids_[h.index], EventKind::Value, denorm});
        lastSent_[h.index] = denorm;
        requestFlushIfPossible();
    }

    void enqueue(const Event& e) { queue_.enqueue(e); }
    void requestFlushIfPossible() { if (requestFlush_) requestFlush_(); }

    ParamController& params_;
    std::vector<clap_id> ids_;
    std::vector<double> lastSent_;
    std::vector<sigslot::scoped_connection> conns_;
    std::function<void()> requestFlush_;
    moodycamel::ConcurrentQueue<Event> queue_{64};
};

} // namespace imagiro
