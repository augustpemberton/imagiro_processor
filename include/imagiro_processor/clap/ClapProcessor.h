#pragma once

#include <clap/clap.h>
#include <clap/helpers/plugin.hh>

#include "../processor/ProcessorCore.h"
#include "../synth/NoteInfo.h"
#include "../synth/NoteTracker.h"
#include "IPluginView.h"
#include "ClapParamBridge.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace imagiro {

// Host-integrated bypass, living in the clap layer where the input signal
// actually exists. When a plugin exposes a "bypass" param the shell drives this
// on the audio thread: it captures the dry input, delays it by the plugin's
// reported latency so it lines up with the (latent) wet output, and runs a short
// equal-power crossfade between wet and delayed-dry whenever bypass toggles. For
// instruments (no input) the dry side is silence, so bypass fades wet to zero.
// Entirely inert — zero cost — for plugins that register no bypass param.
class BypassCrossfade {
public:
    void prepare(int numChannels, int latencySamples, double sampleRate, int maxFrames) {
        channels_   = std::max(0, numChannels);
        latency_    = std::max(0, latencySamples);
        fadeSamples_ = std::max(1, static_cast<int>(sampleRate * kFadeSeconds));
        dry_.assign(channels_, std::vector<float>(std::max(1, maxFrames), 0.f));
        ring_.assign(channels_, std::vector<float>(std::max(1, latency_), 0.f));
        reset();
    }

    void reset() {
        for (auto& c : ring_) std::fill(c.begin(), c.end(), 0.f);
        writePos_ = 0;
        phase_ = 0;
    }

    // Scratch the shell fills with this block's dry input (silence for instruments).
    float* dryChannel(int ch) { return dry_[static_cast<size_t>(ch)].data(); }

    // True mid-transition, so the shell won't sleep while a fade is still moving.
    bool isFading() const { return phase_ > 0 && phase_ < fadeSamples_; }

    // In-place mix over [0,n): wet = out (freshly rendered), dry = captured input
    // delayed by latency. phase_ ramps toward the bypass target one sample at a time.
    void process(float* const* out, int channels, int n, bool bypassed) {
        const int ch = std::min(channels, channels_);
        for (int i = 0; i < n; i++) {
            if (bypassed && phase_ < fadeSamples_) phase_++;
            else if (!bypassed && phase_ > 0) phase_--;

            const float x    = static_cast<float>(phase_) / static_cast<float>(fadeSamples_);
            const float wetG = std::cos(x * kHalfPi);
            const float dryG = std::sin(x * kHalfPi);

            for (int c = 0; c < ch; c++) {
                float dryV;
                if (latency_ == 0) {
                    dryV = dry_[static_cast<size_t>(c)][static_cast<size_t>(i)];
                } else {
                    auto& ring = ring_[static_cast<size_t>(c)];
                    dryV = ring[static_cast<size_t>(writePos_)];
                    ring[static_cast<size_t>(writePos_)] = dry_[static_cast<size_t>(c)][static_cast<size_t>(i)];
                }
                if (out[c]) out[c][i] = wetG * out[c][i] + dryG * dryV;
            }
            if (latency_ > 0) writePos_ = (writePos_ + 1) % latency_;
        }
    }

private:
    static constexpr float kFadeSeconds = 0.02f;   // 20 ms equal-power crossfade
    static constexpr float kHalfPi = 1.57079632679f;

    std::vector<std::vector<float>> dry_;
    std::vector<std::vector<float>> ring_;
    int channels_ = 0;
    int latency_ = 0;
    int fadeSamples_ = 1;
    int writePos_ = 0;
    int phase_ = 0;   // 0 = fully wet, fadeSamples_ = fully bypassed
};

// Frozen FNV-1a (32-bit) over a param uid → stable CLAP param id. This mapping
// must never change: hosts persist automation against these ids.
constexpr uint32_t clapParamId(std::string_view uid) {
    uint32_t h = 2166136261u;
    for (char c : uid) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }
    return h;
}

using ClapPluginBase = clap::helpers::Plugin<
    clap::helpers::MisbehaviourHandler::Terminate,
    clap::helpers::CheckingLevel::Maximal>;

// Reusable headless CLAP shell over a JUCE-free imagiro::ProcessorCore. Owns all
// the generic CLAP glue — param enumeration + frozen id scheme, state streaming,
// transport translation, audio/note port declarations, and the per-block
// event-splitting render loop — and delegates the plugin-specific pieces to
// virtual hooks. A concrete plugin implements only: core() access, its render
// (renderAudio), note handling (handleNoteOn/Off), state json (onSaveState/
// onLoadState), and DSP lifecycle (onActivate/onReset), plus a descriptor and
// clap_entry in its own translation unit.
class ClapProcessor : public ClapPluginBase {
public:
    ClapProcessor(const clap_plugin_descriptor* desc, const clap_host* host)
        : ClapPluginBase(desc, host)
    {
        noteTracker_.onNoteOn    = [this](const NoteInfo& n) { handleNoteOn(n); };
        noteTracker_.onNoteOff   = [this](const NoteInfo& n) { handleNoteOff(n); };
        noteTracker_.onNoteChoke = [this](const NoteInfo& n) { handleNoteChoke(n); };
    }

    // Accessors for tests / harnesses.
    virtual ProcessorCore& core() = 0;
    const ProcessorCore& core() const {
        return const_cast<ClapProcessor*>(this)->core();
    }

    const std::vector<Handle>& paramOrder() const { return paramOrder_; }
    const std::vector<clap_id>& paramIds() const { return paramIds_; }

    // JUCE-free host bridge for UI param editing (gestures + value events).
    // Valid after buildParamIndex(); handed to ivl param bindings by the view.
    HostParamBridge& hostBridge() { return *bridge_; }

    // Smoothed DSP load in [0,1]: process() wall time / block duration. Written
    // on the audio thread, read (relaxed) by the UI for a load readout.
    float cpuLoad() const { return cpuLoad_.load(std::memory_order_relaxed); }

protected:
    // Native embedding window API for this platform. CLAP requires the plugin
    // and host to agree on the windowing API; we only support the OS-native one
    // (no floating windows).
#if defined(_WIN32)
    static constexpr const char* kNativeWindowApi = CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
    static constexpr const char* kNativeWindowApi = CLAP_WINDOW_API_COCOA;
#else
    static constexpr const char* kNativeWindowApi = CLAP_WINDOW_API_X11;
#endif

    // ---- hooks a concrete plugin implements ------------------------------
    // `in` is null for instruments (numAudioInputChannels()==0); for effects it
    // holds numAudioInputChannels() input pointers over the same [0,numFrames)
    // block as `out` (add startSample as with `out`). numChannels is the output
    // channel count; effects must fully write every output sample.
    virtual void renderAudio(const ProcessState& state, const float* const* in,
                             float* const* out, int numChannels, int numFrames,
                             int startSample, int numSamples) = 0;
    virtual void handleNoteOn(const NoteInfo& note) = 0;
    virtual void handleNoteOff(const NoteInfo& note) = 0;
    // Note choke: the host demands the voice(s) stop immediately (e.g. a sample
    // was replaced). Defaults to a release; override for a hard, instant stop.
    virtual void handleNoteChoke(const NoteInfo& note) { handleNoteOff(note); }
    virtual json onSaveState() = 0;
    virtual bool onLoadState(const json& state) = 0;

    // Optional DSP lifecycle hooks; the generic core prepare/reset is handled here.
    virtual void onActivate(double sampleRate, uint32_t maxFrames) {}
    virtual void onDeactivate() {}
    virtual void onReset() {}
    virtual void onBlockStart(const ProcessState& state) {}

    // Audio I/O shape. Overridden by plugins that aren't stereo or that report
    // processing latency to the host.
    virtual uint32_t numChannels() const { return 2; }

    // Number of channels on the main audio input port. 0 (default) declares no
    // input port at all — the instrument shape. Any value > 0 declares one input
    // port with that many channels (the effect shape).
    virtual int numAudioInputChannels() const { return 0; }

    virtual int latencySamples() const { return 0; }

    // Release tail in samples reported via clap.tail. 0 (default) = no tail.
    // Any value >= INT32_MAX means an infinite tail (per the clap.tail spec).
    virtual uint32_t tailSamples() const { return 0; }

    // Whether the plugin accepts note input. Instruments keep the default; audio
    // effects override to false so no note-input port is declared.
    virtual bool wantsNoteInput() const { return true; }

    // Return true when the plugin is fully silent (no active voices, empty tail)
    // so process() can report CLAP_PROCESS_SLEEP and let the host idle it.
    virtual bool wantsSleep() const { return false; }

    // Optional GUI hooks. A plugin with an embeddable editor overrides both:
    // hasGui() -> true and createPluginView() returning its concrete view. When
    // hasGui() is false the whole clap.gui/timer/posix-fd surface stays dark and
    // the shell is headless (used by the test binaries and wasm build).
    virtual bool hasGui() const { return false; }
    virtual std::unique_ptr<IPluginView> createPluginView() { return nullptr; }

    // Build the frozen param id index from core(). Call after core().initParameters().
    void buildParamIndex() {
        paramOrder_.clear();
        paramIds_.clear();
        idToIndex_.clear();
        bypassHandle_ = Handle::invalid();

        core().params().forEach([&](Handle h, const ParamConfig& cfg) {
            const clap_id id = clapParamId(cfg.uid);
            const auto index = static_cast<uint32_t>(paramOrder_.size());

            assert(id != CLAP_INVALID_ID && "param id collides with CLAP_INVALID_ID");
            const bool inserted = idToIndex_.emplace(id, index).second;
            assert(inserted && "duplicate frozen clap param id");
            (void)inserted;

            if (cfg.uid == "bypass") bypassHandle_ = h;

            paramOrder_.push_back(h);
            paramIds_.push_back(id);
        });

        bridge_ = std::make_unique<ClapParamBridge>(core().params(), paramIds_);
        bridge_->setRequestFlush([this] {
            if (_host.canUseParams()) _host.paramsRequestFlush();
        });
    }

    double sampleRate() const { return sampleRate_; }
    NoteTracker& noteTracker() { return noteTracker_; }

    // ---- CLAP lifecycle --------------------------------------------------
    bool init() noexcept override { return true; }

    bool activate(double sampleRate, uint32_t, uint32_t maxFrames) noexcept override {
        sampleRate_ = sampleRate;
        samplesSinceQuiet_ = 0;
        if (bypassHandle_.isValid())
            bypass_.prepare(static_cast<int>(numChannels()), latencySamples(),
                            sampleRate, static_cast<int>(maxFrames));
        onActivate(sampleRate, maxFrames);
        core().transport().update({}, sampleRate);
        return true;
    }

    void deactivate() noexcept override { onDeactivate(); }
    void reset() noexcept override {
        samplesSinceQuiet_ = 0;
        if (bypassHandle_.isValid()) bypass_.reset();
        onReset();
    }

    clap_process_status process(const clap_process* p) noexcept override {
        const auto processStart = std::chrono::steady_clock::now();

        if (p->transport) core().transport().update(toTransportInfo(*p->transport), sampleRate_);
        else core().transport().update({}, sampleRate_);

        const uint32_t nframes = p->frames_count;

        int numCh = 0;
        float* out[kMaxChannels] = {nullptr, nullptr};
        if (p->audio_outputs_count > 0 && p->audio_outputs[0].data32) {
            const uint32_t want = std::min<uint32_t>(numChannels(), kMaxChannels);
            numCh = static_cast<int>(std::min<uint32_t>(want, p->audio_outputs[0].channel_count));
            for (int ch = 0; ch < numCh; ch++) out[ch] = p->audio_outputs[0].data32[ch];
        }

        // Effect-shaped plugins expose an input port; gather its channel pointers.
        const bool hasInput = numAudioInputChannels() > 0;
        const float* inPtrs[kMaxChannels] = {nullptr, nullptr};
        const float* const* in = nullptr;
        if (hasInput && p->audio_inputs_count > 0 && p->audio_inputs[0].data32) {
            const uint32_t want = std::min<uint32_t>(
                static_cast<uint32_t>(numAudioInputChannels()), kMaxChannels);
            const int inCh = static_cast<int>(
                std::min<uint32_t>(want, p->audio_inputs[0].channel_count));
            for (int ch = 0; ch < inCh; ch++) inPtrs[ch] = p->audio_inputs[0].data32[ch];
            in = inPtrs;
        }

        // Capture the dry input before it is overwritten (host may render in place).
        if (bypassHandle_.isValid()) {
            for (int ch = 0; ch < numCh; ch++) {
                float* d = bypass_.dryChannel(ch);
                if (in && in[ch]) std::copy(in[ch], in[ch] + nframes, d);
                else std::fill(d, d + nframes, 0.f);
            }
        }

        // Instruments/generators accumulate into a cleared buffer; effects fully
        // write every output sample, so we must not clear (out may alias input).
        if (!hasInput)
            for (int ch = 0; ch < numCh; ch++)
                if (out[ch]) std::fill(out[ch], out[ch] + nframes, 0.f);

        onBlockStart(core().captureState());

        const auto* evIn = p->in_events;
        const uint32_t nev = evIn ? evIn->size(evIn) : 0;

        auto renderSlice = [&](uint32_t start, uint32_t len) {
            if (len == 0 || numCh == 0) return;
            const auto& state = core().captureState();
            renderAudio(state, in, out, numCh, static_cast<int>(nframes),
                        static_cast<int>(start), static_cast<int>(len));
        };

        uint32_t ev = 0;
        uint32_t pos = 0;
        while (pos < nframes) {
            while (ev < nev) {
                const clap_event_header* h = evIn->get(evIn, ev);
                if (h->time > pos) break;
                processEvent(h, p->out_events);
                ev++;
            }
            uint32_t next = nframes;
            if (ev < nev) {
                const clap_event_header* h = evIn->get(evIn, ev);
                if (h->time < next) next = h->time;
            }
            renderSlice(pos, next - pos);
            pos = next;
        }
        while (ev < nev) {
            processEvent(evIn->get(evIn, ev), p->out_events);
            ev++;
        }

        // Crossfade wet against delayed-dry once the whole block is rendered.
        if (bypassHandle_.isValid() && numCh > 0) {
            const bool bypassed = core().params().getValue(bypassHandle_) >= 0.5f;
            bypass_.process(out, numCh, static_cast<int>(nframes), bypassed);
        }

        if (bridge_) bridge_->flush(p->out_events);

        if (sampleRate_ > 0.0 && nframes > 0) {
            const double procSec = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - processStart).count();
            const double blockSec = nframes / sampleRate_;
            const float inst = static_cast<float>(procSec / blockSec);
            const float prev = cpuLoad_.load(std::memory_order_relaxed);
            cpuLoad_.store(prev + 0.1f * (inst - prev), std::memory_order_relaxed);
        }

        // Sleep only once the plugin is quiet AND its release tail has elapsed. We
        // track samples since wantsSleep() first became true and hold CONTINUE
        // until tailSamples() of quiet have passed (never while a bypass fade is
        // still moving). Any activity resets the counter. An infinite tail
        // (tailSamples() >= INT32_MAX) never sleeps while quiet.
        bool quiet = wantsSleep();
        if (bypassHandle_.isValid() && bypass_.isFading()) quiet = false;
        if (!quiet) {
            samplesSinceQuiet_ = 0;
            return CLAP_PROCESS_CONTINUE;
        }
        const uint32_t tail = tailSamples();
        if (tail == 0) return CLAP_PROCESS_SLEEP;
        samplesSinceQuiet_ += nframes;
        if (tail >= static_cast<uint32_t>(INT32_MAX)) return CLAP_PROCESS_CONTINUE;
        return samplesSinceQuiet_ >= tail ? CLAP_PROCESS_SLEEP : CLAP_PROCESS_CONTINUE;
    }

    // ---- clap.audio-ports: 1 main output, plus 1 main input for effects ------
    // Instruments (numAudioInputChannels()==0) declare output only. Effects add a
    // matching input port; when the channel counts agree both ports advertise an
    // in-place pair so the host may render into the same buffer.
    static constexpr const char* portType(uint32_t ch) {
        return ch == 2 ? CLAP_PORT_STEREO : ch == 1 ? CLAP_PORT_MONO : nullptr;
    }
    bool implementsAudioPorts() const noexcept override { return true; }
    uint32_t audioPortsCount(bool isInput) const noexcept override {
        if (isInput) return numAudioInputChannels() > 0 ? 1u : 0u;
        return 1u;
    }
    bool audioPortsInfo(uint32_t index, bool isInput,
                        clap_audio_port_info* info) const noexcept override {
        if (index != 0) return false;
        const uint32_t outCh = std::min<uint32_t>(numChannels(), kMaxChannels);
        const uint32_t inCh  = std::min<uint32_t>(
            static_cast<uint32_t>(std::max(0, numAudioInputChannels())), kMaxChannels);
        const bool pairable = inCh > 0 && inCh == outCh;

        if (isInput) {
            if (inCh == 0) return false;
            info->id = kInputPortId;
            std::snprintf(info->name, sizeof(info->name), "%s", "Input");
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
            info->channel_count = inCh;
            info->port_type = portType(inCh);
            info->in_place_pair = pairable ? kOutputPortId : CLAP_INVALID_ID;
            return true;
        }
        info->id = kOutputPortId;
        std::snprintf(info->name, sizeof(info->name), "%s", "Output");
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        info->channel_count = outCh;
        info->port_type = portType(outCh);
        info->in_place_pair = pairable ? kInputPortId : CLAP_INVALID_ID;
        return true;
    }

    // ---- clap.latency: reported only when the plugin declares latency --------
    bool implementsLatency() const noexcept override { return latencySamples() > 0; }
    uint32_t latencyGet() const noexcept override {
        return static_cast<uint32_t>(std::max(0, latencySamples()));
    }

    // ---- clap.tail: reported only when the plugin declares a release tail ----
    bool implementsTail() const noexcept override { return tailSamples() > 0; }
    uint32_t tailGet() const noexcept override { return tailSamples(); }

    // ---- clap.note-ports: 1 input for note-driven plugins, none for effects --
    bool implementsNotePorts() const noexcept override { return true; }
    uint32_t notePortsCount(bool isInput) const noexcept override {
        return (isInput && wantsNoteInput()) ? 1u : 0u;
    }
    bool notePortsInfo(uint32_t index, bool isInput,
                       clap_note_port_info* info) const noexcept override {
        if (!isInput || index != 0 || !wantsNoteInput()) return false;
        info->id = 0;
        info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
        info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
        std::snprintf(info->name, sizeof(info->name), "%s", "Notes");
        return true;
    }

    // ---- clap.params -----------------------------------------------------
    bool implementsParams() const noexcept override { return true; }
    uint32_t paramsCount() const noexcept override {
        return static_cast<uint32_t>(paramOrder_.size());
    }
    bool paramsInfo(uint32_t paramIndex, clap_param_info* info) const noexcept override {
        if (paramIndex >= paramOrder_.size()) return false;

        const auto handle = paramOrder_[paramIndex];
        const auto& cfg = core().params().config(handle);

        info->id = paramIds_[paramIndex];
        info->cookie = nullptr;

        uint32_t flags = CLAP_PARAM_IS_AUTOMATABLE;
        if (cfg.range.step_ == 1.f) flags |= CLAP_PARAM_IS_STEPPED;
        if (cfg.isInternal) flags |= CLAP_PARAM_IS_HIDDEN;
        if (handle == bypassHandle_) flags |= CLAP_PARAM_IS_BYPASS;
        info->flags = flags;

        std::snprintf(info->name, sizeof(info->name), "%s", cfg.name.c_str());
        info->module[0] = '\0';

        info->min_value = cfg.range.min_;
        info->max_value = cfg.range.max_;
        info->default_value = cfg.range.clamp(cfg.defaultValue);
        return true;
    }
    bool paramsValue(clap_id paramId, double* value) noexcept override {
        const auto handle = handleForParamId(paramId);
        if (!handle.isValid()) return false;
        *value = core().params().getValue(handle);
        return true;
    }
    bool paramsValueToText(clap_id paramId, double value,
                           char* display, uint32_t size) noexcept override {
        const auto handle = handleForParamId(paramId);
        if (!handle.isValid()) return false;
        const auto text = core().params().getFormatter(handle).toString(static_cast<float>(value));
        std::snprintf(display, size, "%s", text.c_str());
        return true;
    }
    bool paramsTextToValue(clap_id paramId, const char* display,
                           double* value) noexcept override {
        const auto handle = handleForParamId(paramId);
        if (!handle.isValid()) return false;
        const auto parsed = core().params().getFormatter(handle).fromString(display);
        if (!parsed) return false;
        *value = core().params().config(handle).range.clamp(*parsed);
        return true;
    }
    void paramsFlush(const clap_input_events* in,
                     const clap_output_events* out) noexcept override {
        if (in) {
            const uint32_t nev = in->size(in);
            for (uint32_t i = 0; i < nev; i++) {
                const clap_event_header* h = in->get(in, i);
                if (h->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
                if (h->type != CLAP_EVENT_PARAM_VALUE) continue;
                const auto* pv = reinterpret_cast<const clap_event_param_value*>(h);
                const auto handle = handleForParamId(pv->param_id);
                if (handle.isValid()) {
                    core().params().setValue(handle, static_cast<float>(pv->value));
                    if (bridge_) bridge_->noteHostValue(handle, pv->value);
                }
            }
        }
        if (bridge_) bridge_->flush(out);
    }

    // ---- clap.gui --------------------------------------------------------
    bool implementsGui() const noexcept override { return hasGui(); }

    bool guiIsApiSupported(const char* api, bool isFloating) noexcept override {
        if (isFloating) return false;
        return std::strcmp(api, kNativeWindowApi) == 0;
    }
    bool guiGetPreferredApi(const char** api, bool* isFloating) noexcept override {
        *api = kNativeWindowApi;
        *isFloating = false;
        return true;
    }
    bool guiCreate(const char* api, bool isFloating) noexcept override {
        if (isFloating || std::strcmp(api, kNativeWindowApi) != 0) return false;
        if (!view_) view_ = createPluginView();
        if (!view_) return false;

        if (_host.canUseTimerSupport())
            _host.timerSupportRegister(kTimerMs, &timerId_);
        return true;
    }
    void guiDestroy() noexcept override {
        if (timerId_ != CLAP_INVALID_ID && _host.canUseTimerSupport()) {
            _host.timerSupportUnregister(timerId_);
            timerId_ = CLAP_INVALID_ID;
        }
#if defined(__linux__)
        if (view_ && view_->posixFd() >= 0 && _host.canUsePosixFdSupport())
            _host.posixFdSupportUnregister(view_->posixFd());
#endif
        if (view_) {
            view_->destroy();
            view_.reset();
        }
    }
    bool guiSetScale(double scale) noexcept override {
        if (!view_) return false;
        view_->setScale(scale);
        return true;
    }
    bool guiGetSize(uint32_t* width, uint32_t* height) noexcept override {
        if (!view_) return false;
        view_->getSize(*width, *height);
        return true;
    }
    bool guiCanResize() const noexcept override {
        return view_ ? view_->canResize() : false;
    }
    bool guiGetResizeHints(clap_gui_resize_hints_t* hints) noexcept override {
        hints->can_resize_horizontally = true;
        hints->can_resize_vertically = true;
        hints->preserve_aspect_ratio = false;
        hints->aspect_ratio_width = 0;
        hints->aspect_ratio_height = 0;
        return true;
    }
    bool guiAdjustSize(uint32_t* width, uint32_t* height) noexcept override {
        return view_ != nullptr;   // accept host-proposed size as-is
    }
    bool guiSetSize(uint32_t width, uint32_t height) noexcept override {
        if (!view_) return false;
        view_->resize(width, height);
        return true;
    }
    bool guiSetParent(const clap_window* window) noexcept override {
        if (!view_ || !window) return false;
        uint32_t w = 0, h = 0;
        view_->getSize(w, h);
        view_->embed(window->ptr, w, h);
#if defined(__linux__)
        if (_host.canUsePosixFdSupport() && view_->posixFd() >= 0) {
            const clap_posix_fd_flags_t flags =
                CLAP_POSIX_FD_READ | CLAP_POSIX_FD_WRITE | CLAP_POSIX_FD_ERROR;
            return _host.posixFdSupportRegister(view_->posixFd(), flags);
        }
#endif
        return true;
    }
    bool guiShow() noexcept override {
        if (!view_) return false;
        view_->show();
        return true;
    }
    bool guiHide() noexcept override {
        if (!view_) return false;
        view_->hide();
        return true;
    }

    // ---- clap.timer-support: drives the UI tick (snapshot sync + animation) ---
    bool implementsTimerSupport() const noexcept override { return hasGui(); }
    void onTimer(clap_id timerId) noexcept override {
        if (timerId != timerId_) return;
        // Fallback driver only: the view's own vsync loop owns the frame tick
        // (and the UI param dispatch it carries). view_->tick() no-ops when that
        // loop is live, so this just covers hidden/unpainted windows.
        if (view_) view_->tick();
    }

    // ---- clap.posix-fd-support (Linux): pumps the visage window render loop ---
#if defined(__linux__)
    bool implementsPosixFdSupport() const noexcept override { return hasGui(); }
    void onPosixFd(int, clap_posix_fd_flags_t) noexcept override {
        if (view_) view_->onPosixFd();
    }
#endif

    // ---- clap.state ------------------------------------------------------
    bool implementsState() const noexcept override { return true; }
    bool stateSave(const clap_ostream* stream) noexcept override {
        core().params().dispatchUIChanges();
        const std::string blob = onSaveState().dump();

        const char* data = blob.data();
        uint64_t remaining = blob.size();
        while (remaining > 0) {
            const int64_t written = stream->write(stream, data, remaining);
            if (written <= 0) return false;
            data += written;
            remaining -= static_cast<uint64_t>(written);
        }
        return true;
    }
    bool stateLoad(const clap_istream* stream) noexcept override {
        std::string blob;
        char buf[4096];
        for (;;) {
            const int64_t got = stream->read(stream, buf, sizeof(buf));
            if (got < 0) return false;
            if (got == 0) break;
            blob.append(buf, static_cast<size_t>(got));
        }

        try {
            if (!onLoadState(json::parse(blob))) return false;
        } catch (...) {
            return false;
        }
        return true;
    }

private:
    static NoteInfo toNoteInfo(const clap_event_note& n) {
        const int channel = (n.channel < 0) ? 0 : n.channel;   // clap 0-based, NoteInfo 1-based
        const int key = (n.key < 0) ? 60 : n.key;
        const int noteId = (n.note_id >= 0) ? n.note_id : ((channel + 1) << 7) + key;
        return NoteInfo{
            noteId,
            channel + 1,
            key,
            static_cast<float>(n.velocity)
        };
    }

    static TransportInfo toTransportInfo(const clap_event_transport& t) {
        TransportInfo info;
        if (t.flags & CLAP_TRANSPORT_HAS_TEMPO) info.bpm = t.tempo;
        info.isPlaying = (t.flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
        if (t.flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE) {
            info.timeSigNumerator = t.tsig_num;
            info.timeSigDenominator = t.tsig_denom;
        }
        if (t.flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) {
            info.ppqPosition = static_cast<double>(t.song_pos_beats)
                             / static_cast<double>(CLAP_BEATTIME_FACTOR);
        }
        return info;
    }

    Handle handleForParamId(clap_id id) const {
        if (auto it = idToIndex_.find(id); it != idToIndex_.end())
            return paramOrder_[it->second];
        return Handle::invalid();
    }

    void processEvent(const clap_event_header* h, const clap_output_events*) {
        if (h->space_id != CLAP_CORE_EVENT_SPACE_ID) return;

        // Both dialects flow through noteTracker_ so sustain (CC64) and the
        // held-note bookkeeping apply uniformly whichever the host sends.
        switch (h->type) {
            case CLAP_EVENT_NOTE_ON:
                noteTracker_.noteOn(toNoteInfo(*reinterpret_cast<const clap_event_note*>(h)));
                break;
            case CLAP_EVENT_NOTE_OFF:
                noteTracker_.noteOff(toNoteInfo(*reinterpret_cast<const clap_event_note*>(h)));
                break;
            case CLAP_EVENT_NOTE_CHOKE:
                noteTracker_.noteChoke(toNoteInfo(*reinterpret_cast<const clap_event_note*>(h)));
                break;
            case CLAP_EVENT_MIDI: {
                const auto* m = reinterpret_cast<const clap_event_midi*>(h);
                noteTracker_.processMessage(m->data[0], m->data[1], m->data[2]);
                break;
            }
            case CLAP_EVENT_PARAM_VALUE: {
                const auto* pv = reinterpret_cast<const clap_event_param_value*>(h);
                const auto handle = handleForParamId(pv->param_id);
                if (handle.isValid()) {
                    core().params().setValue(handle, static_cast<float>(pv->value));
                    if (bridge_) bridge_->noteHostValue(handle, pv->value);
                }
                break;
            }
            default:
                break;
        }
    }

    NoteTracker noteTracker_;
    double sampleRate_ = 44100.0;
    std::atomic<float> cpuLoad_{0.f};

    std::vector<Handle> paramOrder_;
    std::vector<clap_id> paramIds_;
    std::unordered_map<clap_id, uint32_t> idToIndex_;

    Handle bypassHandle_ = Handle::invalid();
    BypassCrossfade bypass_;
    uint64_t samplesSinceQuiet_ = 0;

    std::unique_ptr<ClapParamBridge> bridge_;
    std::unique_ptr<IPluginView> view_;
    clap_id timerId_ = CLAP_INVALID_ID;
    static constexpr uint32_t kTimerMs = 16;
    static constexpr uint32_t kMaxChannels = 2;
    static constexpr clap_id kOutputPortId = 0;
    static constexpr clap_id kInputPortId = 1;
};

} // namespace imagiro
