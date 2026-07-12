// ProcessorCore.h
#pragma once

#include "TransportState.h"
#include "BypassMixer.h"
#include "state/ProcessState.h"

#include "../preset/Preset.h"
#include "../parameter/ParamController.h"
#include "../parameter/ParamValue.h"

namespace imagiro {

class ProcessorCore {
public:
    void initParameters() {
        audioThreadState_.params().resize(paramController_.size());

        if (paramController_.has("bypass")) {
            bypassHandle_ = paramController_.handle("bypass");
        }

        if (paramController_.has("mix")) {
            mixHandle_ = paramController_.handle("mix");
        }
    }

    void prepare(double sampleRate, unsigned int numOutputChannels, int latencySamples) {
        bypassMixer_.prepare(sampleRate, numOutputChannels);
        bypassMixer_.setLatency(latencySamples);

        if (firstPrepare_) {
            bypassMixer_.skipSmoothing();
            firstPrepare_ = false;
        }
    }

    const ProcessState& captureState() {
        audioThreadState_.setBpm(transport_.bpm());
        audioThreadState_.setSampleRate(transport_.sampleRate());
        paramController_.snapshotInto(audioThreadState_.params());
        paramController_.dispatchAudioChanges();
        return audioThreadState_;
    }

    void updateBypassFromState(const ProcessState& state) {
        if (bypassHandle_.isValid()) {
            bypassMixer_.setBypass(state.value(bypassHandle_) > 0.5f);
        }
        if (mixHandle_.isValid()) {
            bypassMixer_.setMix(state.value(mixHandle_));
        }
    }

    json stateToJson() const {
        json j = json::object();
        j["params"] = paramController_.registryUI();  // Uses to_json for StateRegistry
        return j;
    }

    void loadStateFromJson(const json& j) {
        if (j.contains("params")) {
            auto reg = paramController_.registryUI().fromJson(j["params"]);
            paramController_.setRegistryUI(std::move(reg));
        }
    }

    Preset savePreset(PresetMetadata metadata = {}) const {
        Preset preset(std::move(metadata));
        preset.state() = stateToJson();
        return preset;
    }

    void loadPreset(const Preset& preset) {
        loadStateFromJson(preset.state());
    }

    ParamController& params() { return paramController_; }
    const ParamController& params() const { return paramController_; }

    TransportState& transport() { return transport_; }
    const TransportState& transport() const { return transport_; }

    ProcessState& audioThreadState() { return audioThreadState_; }
    BypassMixer& bypassMixer() { return bypassMixer_; }
    Handle& bypassHandle() { return bypassHandle_; }
    Handle& mixHandle() { return mixHandle_; }

    double bpm() const { return transport_.bpm(); }
    double sampleRate() const { return transport_.sampleRate(); }

private:
    ProcessState audioThreadState_;
    ParamController paramController_;
    TransportState transport_;
    BypassMixer bypassMixer_;
    Handle bypassHandle_;
    Handle mixHandle_;
    bool firstPrepare_{true};
};

} // namespace imagiro
