// Standalone-only implementation of ivl::IDeviceSettings over clap-wrapper's
// StandaloneHost. This translation unit is compiled ONLY into standalone wrapper
// targets (see imagiro_add_clap_wrapper), because getStandaloneHost() and the
// RtAudio/RtMidi backend symbols exist only there. Other plugin formats never
// see it, so they link without any standalone dependency.
//
// It registers itself into ivl::DeviceSettingsRegistry at load time; the shared
// GUI then discovers device settings are available via
// ivl::DeviceSettingsRegistry::instance() != nullptr.

#include <ivl/components/DeviceSettings.h>

#include "detail/standalone/standalone_host.h"
#include "detail/standalone/entry.h"

#include <algorithm>

namespace {

using freeaudio::clap_wrapper::standalone::StandaloneHost;
using freeaudio::clap_wrapper::standalone::getStandaloneHost;
using freeaudio::clap_wrapper::standalone::getStandaloneSettingsPath;

class StandaloneDeviceSettings final : public ivl::IDeviceSettings {
public:
    std::vector<ivl::DeviceOption> outputDevices() override {
        return toOptions(withHost(&StandaloneHost::getOutputAudioDevices));
    }
    unsigned int currentOutputDevice() override {
        auto* h = host();
        return h ? h->audioOutputDeviceID : 0;
    }
    void setOutputDevice(unsigned int id) override {
        auto* h = host();
        if (!h) return;
        h->audioOutputDeviceID = id;
        h->totalOutputChannels = h->rtaDac->getDeviceInfo(id).outputChannels;
        restartAudio();
        persist();
    }

    std::vector<ivl::DeviceOption> inputDevices() override {
        return toOptions(withHost(&StandaloneHost::getInputAudioDevices));
    }
    unsigned int currentInputDevice() override {
        auto* h = host();
        return h ? h->audioInputDeviceID : 0;
    }
    void setInputDevice(unsigned int id) override {
        auto* h = host();
        if (!h) return;
        h->audioInputDeviceID = id;
        h->totalInputChannels = h->rtaDac->getDeviceInfo(id).inputChannels;
        restartAudio();
        persist();
    }
    bool inputEnabled() override {
        auto* h = host();
        return h && h->audioInputUsed;
    }
    void setInputEnabled(bool enabled) override {
        auto* h = host();
        if (!h) return;
        h->audioInputUsed = enabled;
        restartAudio();
        persist();
    }
    bool hasInputBus() override {
        auto* h = host();
        return h && h->numAudioInputs > 0;
    }

    std::vector<int> sampleRates() override {
        auto* h = host();
        if (!h) return {};
        h->guaranteeRtAudioDAC();
        std::vector<int> res;
        for (auto sr : h->rtaDac->getDeviceInfo(h->audioOutputDeviceID).sampleRates)
            res.push_back(static_cast<int>(sr));
        return res;
    }
    int currentSampleRate() override {
        auto* h = host();
        return h ? h->currentSampleRate : 0;
    }
    void setSampleRate(int sampleRate) override {
        auto* h = host();
        if (!h) return;
        h->currentSampleRate = sampleRate;
        restartAudio();
        persist();
    }

    bool supportsBufferSize() override { return true; }
    std::vector<unsigned int> bufferSizes() override {
        auto* h = host();
        return h ? h->getBufferSizes() : std::vector<unsigned int>{};
    }
    unsigned int currentBufferSize() override {
        auto* h = host();
        return h ? h->currentBufferSize : 0;
    }
    void setBufferSize(unsigned int frames) override {
        auto* h = host();
        if (!h) return;
        h->currentBufferSize = frames;
        restartAudio();
        persist();
    }

    std::vector<ivl::MidiPortState> midiPorts() override {
        syncMidiState();
        std::vector<ivl::MidiPortState> res;
        try {
            RtMidiIn probe;
            const unsigned int count = probe.getPortCount();
            for (unsigned int i = 0; i < count; ++i) {
                const bool enabled = i < enabled_.size() ? enabled_[i] : false;
                res.push_back({probe.getPortName(i), enabled});
            }
        } catch (RtMidiError&) {
        }
        return res;
    }
    void setMidiPortEnabled(int index, bool enabled) override {
        syncMidiState();
        if (index < 0 || index >= static_cast<int>(enabled_.size())) return;
        enabled_[index] = enabled;
        rebuildMidi();
        persist();
    }

private:
    static StandaloneHost* host() { return getStandaloneHost(); }

    template <typename Method>
    static auto withHost(Method m) -> decltype((std::declval<StandaloneHost>().*m)()) {
        auto* h = host();
        if (!h) return {};
        return (h->*m)();
    }

    static std::vector<ivl::DeviceOption> toOptions(const std::vector<RtAudio::DeviceInfo>& devices) {
        std::vector<ivl::DeviceOption> res;
        res.reserve(devices.size());
        for (const auto& d : devices) res.push_back({d.ID, d.name});
        return res;
    }

    void restartAudio() {
        auto* h = host();
        if (!h) return;
        const bool useOut = h->audioOutputUsed && h->numAudioOutputs > 0;
        const bool useIn = h->audioInputUsed && h->numAudioInputs > 0;
        h->startAudioThreadOn(h->audioInputDeviceID, 2, useIn, h->audioOutputDeviceID, 2, useOut,
                              h->currentSampleRate);
    }

    // clap-wrapper opens every MIDI port on launch but leaves currentMidiPorts
    // empty, so on first access we treat all ports as enabled without disturbing
    // the already-open streams.
    void syncMidiState() {
        if (midiInitialized_) return;
        midiInitialized_ = true;
        try {
            RtMidiIn probe;
            enabled_.assign(probe.getPortCount(), true);
        } catch (RtMidiError&) {
            enabled_.clear();
        }
    }

    // Mirror windows_standalone.cpp: tear down all open inputs, then reopen the
    // enabled set, wiring each to the host's midiCallback exactly as the wrapper does.
    void rebuildMidi() {
        auto* h = host();
        if (!h) return;
        for (auto& in : h->midiIns) in.reset();
        h->midiIns.clear();
        h->currentMidiPorts.clear();
        for (unsigned int i = 0; i < enabled_.size(); ++i) {
            if (!enabled_[i]) continue;
            try {
                auto in = std::make_unique<RtMidiIn>();
                in->openPort(i);
                in->setCallback(StandaloneHost::midiCallback, h);
                h->midiIns.push_back(std::move(in));
                h->currentMidiPorts.push_back(i);
            } catch (RtMidiError&) {
            }
        }
    }

    void persist() {
        auto* h = host();
        if (!h) return;
        if (auto path = getStandaloneSettingsPath())
            h->saveStandaloneAndPluginSettings(*path, "defaults.clapwrapper");
    }

    std::vector<bool> enabled_;
    bool midiInitialized_ = false;
};

StandaloneDeviceSettings g_deviceSettings;

struct DeviceSettingsRegistrar {
    DeviceSettingsRegistrar() { ivl::DeviceSettingsRegistry::setInstance(&g_deviceSettings); }
} g_deviceSettingsRegistrar;

} // namespace
