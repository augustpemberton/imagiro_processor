// ProcessorCore.h
#pragma once

#include "TransportState.h"
#include "state/ProcessState.h"

#include "../preset/Preset.h"
#include "../parameter/ParamController.h"
#include "../parameter/ParamValue.h"

namespace imagiro {

class ProcessorCore {
public:
    void initParameters() {
        audioThreadState_.params().resize(paramController_.size());
    }

    const ProcessState& captureState() {
        audioThreadState_.setBpm(transport_.bpm());
        audioThreadState_.setSampleRate(transport_.sampleRate());
        paramController_.snapshotInto(audioThreadState_.params());
        return audioThreadState_;
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

    double bpm() const { return transport_.bpm(); }
    double sampleRate() const { return transport_.sampleRate(); }

private:
    ProcessState audioThreadState_;
    ParamController paramController_;
    TransportState transport_;
};

} // namespace imagiro
