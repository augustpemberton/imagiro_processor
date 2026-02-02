// Processor.h
#pragma once

#include "ProcessorBase.h"
#include "TransportState.h"
#include "BypassMixer.h"

#include "../preset/Preset.h"
#include "imagiro_processor/parameter/JuceParamAdapter.h"
#include "imagiro_processor/parameter/ParamController.h"
#include "imagiro_processor/parameter/ParamValue.h"
#include "state/ProcessState.h"

namespace imagiro {

class Processor : public ProcessorBase, juce::Timer {
public:
    explicit Processor(const BusesProperties& ioLayout = getDefaultProperties())
        : ProcessorBase(ioLayout) {
        startTimerHz(120);
    }

    void initParameters() {
        juceAdapter_ = std::make_unique<JuceParamAdapter>(paramController_, *this);

        if (paramController_.has("bypass")) {
            bypassHandle_ = paramController_.handle("bypass");
        }

        if (paramController_.has("mix")) {
            mixHandle_ = paramController_.handle("mix");
        }
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        bypassMixer_.prepare(sampleRate, getTotalNumOutputChannels());
        bypassMixer_.setLatency(getLatencySamples());

        if (firstPrepare_) {
            bypassMixer_.skipSmoothing();
            firstPrepare_ = false;
        }
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) final {
        transport_.update(getPlayHead(), getSampleRate());
        juceAdapter_->pullFromHost();

        const auto state = captureState();

        if (bypassHandle_.isValid()) {
            bypassMixer_.setBypass(state.value(bypassHandle_) > 0.5f);
        }
        if (mixHandle_.isValid()) {
            bypassMixer_.setMix(state.value(mixHandle_));
        }

        bypassMixer_.pushDry(buffer);

        if (bypassMixer_.isProcessingNeeded()) {
            process(buffer, midi, state);
        }

        bypassMixer_.applyMix(buffer);
    }

    void timerCallback() override {
        params().dispatchUIChanges();
    }

    Preset savePreset(PresetMetadata metadata = {}) const {
        Preset preset(std::move(metadata));
        preset.state() = stateToJson();
        return preset;
    }

    void loadPreset(const Preset& preset) {
        loadStateFromJson(preset.state());
    }

    void getStateInformation(juce::MemoryBlock& destData) override {
        const auto blob = savePreset().toBinary();
        destData.replaceAll(blob.data(), blob.size());
    }

    void setStateInformation(const void* data, int sizeInBytes) override {
        if (const auto preset = Preset::fromBinary(static_cast<const uint8_t*>(data),
                                              static_cast<size_t>(sizeInBytes))) {
            loadPreset(*preset);
        }
    }

    ParamController& params() { return paramController_; }
    const ParamController& params() const { return paramController_; }

    JuceParamAdapter* juceAdapter() const { return juceAdapter_.get(); }
    TransportState& transport() { return transport_; }

protected:
    virtual void process(juce::AudioBuffer<float>& buffer,
                        juce::MidiBuffer& midi,
                        const ProcessState& state) = 0;

    virtual ProcessState captureState() {
        audioThreadState_.setBpm(transport_.bpm());
        audioThreadState_.setSampleRate(transport_.sampleRate());
        audioThreadState_.params() = paramController_.captureAudio();  // Lock-free atomic load + copy
        return audioThreadState_;
    }

    virtual json stateToJson() const {
        json j = json::object();
        j["params"] = paramController_.registryUI();  // Uses to_json for StateRegistry
        return j;
    }

    virtual void loadStateFromJson(const json& j) {
        if (j.contains("params")) {
            auto reg = paramController_.registryUI().fromJson(j["params"]);
            paramController_.setRegistryUI(std::move(reg));
        }
    }

    ProcessState audioThreadState_;
    ParamController paramController_;

    std::unique_ptr<JuceParamAdapter> juceAdapter_;
    TransportState transport_;

    BypassMixer bypassMixer_;
    Handle bypassHandle_;
    Handle mixHandle_;
    
    bool firstPrepare_{true};
};

} // namespace imagiro