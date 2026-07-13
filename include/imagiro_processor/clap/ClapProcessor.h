#pragma once

#include <clap/clap.h>
#include <clap/helpers/plugin.hh>

#include "../processor/ProcessorCore.h"
#include "../synth/NoteInfo.h"
#include "../synth/NoteTracker.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace imagiro {

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
        noteTracker_.onNoteOn  = [this](const NoteInfo& n) { handleNoteOn(n); };
        noteTracker_.onNoteOff = [this](const NoteInfo& n) { handleNoteOff(n); };
    }

    // Accessors for tests / harnesses.
    virtual ProcessorCore& core() = 0;
    const ProcessorCore& core() const {
        return const_cast<ClapProcessor*>(this)->core();
    }

    const std::vector<Handle>& paramOrder() const { return paramOrder_; }
    const std::vector<clap_id>& paramIds() const { return paramIds_; }

protected:
    // ---- hooks a concrete plugin implements ------------------------------
    virtual void renderAudio(const ProcessState& state, float* const* out,
                             int numChannels, int numFrames,
                             int startSample, int numSamples) = 0;
    virtual void handleNoteOn(const NoteInfo& note) = 0;
    virtual void handleNoteOff(const NoteInfo& note) = 0;
    virtual json onSaveState() = 0;
    virtual bool onLoadState(const json& state) = 0;

    // Optional DSP lifecycle hooks; the generic core prepare/reset is handled here.
    virtual void onActivate(double sampleRate, uint32_t maxFrames) {}
    virtual void onDeactivate() {}
    virtual void onReset() {}
    virtual void onBlockStart(const ProcessState& state) {}

    // Build the frozen param id index from core(). Call after core().initParameters().
    void buildParamIndex() {
        paramOrder_.clear();
        paramIds_.clear();
        idToIndex_.clear();

        core().params().forEach([&](Handle h, const ParamConfig& cfg) {
            const clap_id id = clapParamId(cfg.uid);
            const auto index = static_cast<uint32_t>(paramOrder_.size());

            assert(id != CLAP_INVALID_ID && "param id collides with CLAP_INVALID_ID");
            const bool inserted = idToIndex_.emplace(id, index).second;
            assert(inserted && "duplicate frozen clap param id");
            (void)inserted;

            paramOrder_.push_back(h);
            paramIds_.push_back(id);
        });
    }

    double sampleRate() const { return sampleRate_; }
    NoteTracker& noteTracker() { return noteTracker_; }

    // ---- CLAP lifecycle --------------------------------------------------
    bool init() noexcept override { return true; }

    bool activate(double sampleRate, uint32_t, uint32_t maxFrames) noexcept override {
        sampleRate_ = sampleRate;
        onActivate(sampleRate, maxFrames);
        core().prepare(sampleRate, 2, 0);
        core().transport().update({}, sampleRate);
        return true;
    }

    void deactivate() noexcept override { onDeactivate(); }
    void reset() noexcept override { onReset(); }

    clap_process_status process(const clap_process* p) noexcept override {
        if (p->transport) core().transport().update(toTransportInfo(*p->transport), sampleRate_);
        else core().transport().update({}, sampleRate_);

        const uint32_t nframes = p->frames_count;

        int numCh = 0;
        float* out[2] = {nullptr, nullptr};
        if (p->audio_outputs_count > 0 && p->audio_outputs[0].data32) {
            numCh = static_cast<int>(std::min<uint32_t>(2, p->audio_outputs[0].channel_count));
            for (int ch = 0; ch < numCh; ch++) out[ch] = p->audio_outputs[0].data32[ch];
        }
        for (int ch = 0; ch < numCh; ch++)
            if (out[ch]) std::fill(out[ch], out[ch] + nframes, 0.f);

        onBlockStart(core().captureState());

        const auto* in = p->in_events;
        const uint32_t nev = in ? in->size(in) : 0;

        auto renderSlice = [&](uint32_t start, uint32_t len) {
            if (len == 0 || numCh == 0) return;
            const auto& state = core().captureState();
            renderAudio(state, out, numCh, static_cast<int>(nframes),
                        static_cast<int>(start), static_cast<int>(len));
        };

        uint32_t ev = 0;
        uint32_t pos = 0;
        while (pos < nframes) {
            while (ev < nev) {
                const clap_event_header* h = in->get(in, ev);
                if (h->time > pos) break;
                processEvent(h, p->out_events);
                ev++;
            }
            uint32_t next = nframes;
            if (ev < nev) {
                const clap_event_header* h = in->get(in, ev);
                if (h->time < next) next = h->time;
            }
            renderSlice(pos, next - pos);
            pos = next;
        }
        while (ev < nev) {
            processEvent(in->get(in, ev), p->out_events);
            ev++;
        }

        return CLAP_PROCESS_CONTINUE;
    }

    // ---- clap.audio-ports: 0 inputs, 1 stereo output ---------------------
    bool implementsAudioPorts() const noexcept override { return true; }
    uint32_t audioPortsCount(bool isInput) const noexcept override { return isInput ? 0u : 1u; }
    bool audioPortsInfo(uint32_t index, bool isInput,
                        clap_audio_port_info* info) const noexcept override {
        if (isInput || index != 0) return false;
        info->id = 0;
        std::snprintf(info->name, sizeof(info->name), "%s", "Output");
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        info->channel_count = 2;
        info->port_type = CLAP_PORT_STEREO;
        info->in_place_pair = CLAP_INVALID_ID;
        return true;
    }

    // ---- clap.note-ports: 1 input, prefers CLAP dialect, accepts MIDI ----
    bool implementsNotePorts() const noexcept override { return true; }
    uint32_t notePortsCount(bool isInput) const noexcept override { return isInput ? 1u : 0u; }
    bool notePortsInfo(uint32_t index, bool isInput,
                       clap_note_port_info* info) const noexcept override {
        if (!isInput || index != 0) return false;
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
                     const clap_output_events*) noexcept override {
        if (!in) return;
        const uint32_t nev = in->size(in);
        for (uint32_t i = 0; i < nev; i++) {
            const clap_event_header* h = in->get(in, i);
            if (h->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
            if (h->type != CLAP_EVENT_PARAM_VALUE) continue;
            const auto* pv = reinterpret_cast<const clap_event_param_value*>(h);
            const auto handle = handleForParamId(pv->param_id);
            if (handle.isValid())
                core().params().setValue(handle, static_cast<float>(pv->value));
        }
    }

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

        switch (h->type) {
            case CLAP_EVENT_NOTE_ON:
                handleNoteOn(toNoteInfo(*reinterpret_cast<const clap_event_note*>(h)));
                break;
            case CLAP_EVENT_NOTE_OFF:
            case CLAP_EVENT_NOTE_CHOKE:
                handleNoteOff(toNoteInfo(*reinterpret_cast<const clap_event_note*>(h)));
                break;
            case CLAP_EVENT_MIDI: {
                const auto* m = reinterpret_cast<const clap_event_midi*>(h);
                noteTracker_.processMessage(m->data[0], m->data[1], m->data[2]);
                break;
            }
            case CLAP_EVENT_PARAM_VALUE: {
                const auto* pv = reinterpret_cast<const clap_event_param_value*>(h);
                const auto handle = handleForParamId(pv->param_id);
                if (handle.isValid())
                    core().params().setValue(handle, static_cast<float>(pv->value));
                break;
            }
            default:
                break;
        }
    }

    NoteTracker noteTracker_;
    double sampleRate_ = 44100.0;

    std::vector<Handle> paramOrder_;
    std::vector<clap_id> paramIds_;
    std::unordered_map<clap_id, uint32_t> idToIndex_;
};

} // namespace imagiro
