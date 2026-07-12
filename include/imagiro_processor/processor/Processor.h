// Processor.h
#pragma once

#include "ProcessorBase.h"
#include "ProcessorCore.h"
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
        juceAdapter_ = std::make_unique<JuceParamAdapter>(core_.params(), *this);
        core_.initParameters();
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        core_.prepare(sampleRate,
                      static_cast<unsigned int>(getTotalNumOutputChannels()),
                      getLatencySamples());
    }

    static TransportInfo makeTransportInfo(const juce::AudioPlayHead* playhead) {
        TransportInfo info;
        if (!playhead) return info;

        auto pos = playhead->getPosition();
        if (!pos) return info;

        if (auto bpm = pos->getBpm()) info.bpm = *bpm;
        if (pos->getIsPlaying()) info.isPlaying = true;
        if (auto timeSig = pos->getTimeSignature()) {
            info.timeSigNumerator = timeSig->numerator;
            info.timeSigDenominator = timeSig->denominator;
        }
        if (auto ppq = pos->getPpqPosition()) info.ppqPosition = *ppq;

        return info;
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) final {
        transport_.update(makeTransportInfo(getPlayHead()), getSampleRate());
        juceAdapter_->pullFromHost();

        const auto& state = captureState();
        core_.updateBypassFromState(state);

        bypassMixer_.pushDry(buffer.getArrayOfReadPointers(), buffer.getNumSamples());

        if (bypassMixer_.isProcessingNeeded()) {
            process(buffer, midi, state);
            afterProcess();
        }

        bypassMixer_.applyMix(buffer.getArrayOfWritePointers(), buffer.getNumSamples());
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

    ParamController& params() { return core_.params(); }
    const ParamController& params() const { return core_.params(); }

    JuceParamAdapter* juceAdapter() const { return juceAdapter_.get(); }
    TransportState& transport() { return core_.transport(); }

    ProcessorCore& core() { return core_; }
    const ProcessorCore& core() const { return core_; }

protected:
    virtual void process(juce::AudioBuffer<float>& buffer,
                        juce::MidiBuffer& midi,
                        const ProcessState& state) = 0;

    virtual void afterProcess() {}

    virtual const ProcessState& captureState() {
        return core_.captureState();
    }

    virtual json stateToJson() const {
        return core_.stateToJson();
    }

    virtual void loadStateFromJson(const json& j) {
        core_.loadStateFromJson(j);
    }

    ProcessorCore core_;

    std::unique_ptr<JuceParamAdapter> juceAdapter_;

    ProcessState& audioThreadState_ = core_.audioThreadState();
    ParamController& paramController_ = core_.params();
    TransportState& transport_ = core_.transport();
    BypassMixer& bypassMixer_ = core_.bypassMixer();
    Handle& bypassHandle_ = core_.bypassHandle();
    Handle& mixHandle_ = core_.mixHandle();
};

} // namespace imagiro
