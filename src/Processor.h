//
// Created by August Pemberton on 12/08/2022.
//

#pragma once
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <imagiro_util/imagiro_util.h>
#include "ProcessorBase.h"
#include "valuedata/ValueData.h"
#include "valuedata/ValueData.h"
#include "parameter/Parameter.h"
#include "preset/Preset.h"
#include "config/AuthorizationManager.h"
#include "imagiro_processor/src/preset/FileBackedPreset.h"
#include "imagiro_processor/src/config/Resources.h"
#include "imagiro_processor/src/parameter/ParameterLoader.h"

class Preset;

namespace imagiro {
    class Processor : public ProcessorBase, protected juce::MultiTimer, ValueData::Listener, public Parameter::Listener, private juce::AudioProcessorListener {
    public:
        Processor(juce::String parametersYAMLString,
                  const ParameterLoader& loader = ParameterLoader(),
                  const juce::AudioProcessor::BusesProperties& ioLayout = getDefaultProperties());

        ~Processor() override;

        // =================================================================
        struct BPMListener {
            virtual void bpmChanged(double newBPM) {}
            virtual void sampleRateChanged(double newRate) {}
            virtual void playChanged(bool isPlaying) {}
        };

        void addBPMListener(BPMListener* l) { bpmListeners.add(l); }
        void removeBPMListener(BPMListener* l) { bpmListeners.remove(l); }
        void setDefaultBPM(double bpm) {defaultBPM = bpm;}
        double getDefaultBPM() {return defaultBPM;}

        // =================================================================

        void reset() override;

        struct ParameterListener {
            virtual ~ParameterListener() = default;
            virtual void onParameterAdded(Parameter& p) {}
        };

        void addParameterListener(ParameterListener* l) { parameterListeners.add(l); }
        void removeParameterListener(ParameterListener* l) { parameterListeners.remove(l); }

        Parameter* addParam (std::unique_ptr<Parameter> p);
        Parameter* getParameter (const juce::String& uid);
        const juce::Array<Parameter*>& getPluginParameters();
        void timerCallback(int timerID) override;

        // =================================================================
        choc::value::Value handleMessage(const std::string& type, const choc::value::ValueView& data);
        virtual choc::value::Value OnMessageReceived(std::string type, const choc::value::ValueView& data) {return {};}

        struct MessageListener {
            virtual ~MessageListener() = default;
            virtual void OnProcessorMessage(Processor& processor, const std::string& type, const choc::value::ValueView& data) {}
        };

        void addMessageListener(MessageListener* l) {messageListeners.add(l);}
        void removeMessageListener(MessageListener* l) {messageListeners.remove(l);}

        ValueData& getValueData() { return valueData; }

        // =================================================================
        struct PresetListener {
            virtual void OnPresetChange(Preset& preset) {}
        };

        void addPresetListener(PresetListener *l);
        void removePresetListener(PresetListener *l);

        void queuePreset(const FileBackedPreset& preset, bool loadOnAudioThread = false);
        void queuePreset(const Preset& preset, bool loadOnAudioThread = false);

        /*!
         * Creates a preset from the processor's current settings.
         * @param name The name of the preset
         * @param isDAWSaveState Whether or not this preset is used for the daw to save plugin state.
         * If false, you may wish to add extra properties (preset author, preset name etc).
         * @return The created preset
         */
        virtual Preset createPreset(const juce::String &name, bool isDAWSaveState);
        virtual void loadPreset(Preset preset);
        virtual void loadPreset(FileBackedPreset preset);

        virtual std::optional<std::string> isPresetAvailable(Preset& preset);

        void getStateInformation(juce::MemoryBlock &destData) override;
        void setStateInformation(const void *data, int sizeInBytes) override;

        std::optional<FileBackedPreset> lastLoadedPreset;

        // =================================================================
        double getLastSampleRate() const {return lastSampleRate; }

        double getSyncTimeSeconds(float proportionOfBeat);
        double getLastBPM() const;
        static double getNoteLengthSamples(double bpm, float proportionOfBeat, double sampleRate);
        double getNoteLengthSamples(float proportionOfBeat) const;
        static float getSamplesPerBeat(float bpm, double sampleRate);
        float getSamplesPerBeat() const;
        float getNotes(float seconds);
        float getNotesFromSamples(float samples);

        juce::Optional<juce::AudioPlayHead::PositionInfo>& getPosition() { return posInfo; }
        juce::AudioProcessor::TrackProperties getProperties() { return trackProperties; }

        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override;

        void parameterChanged(imagiro::Parameter *param) override;
        void configChanged(imagiro::Parameter *param) override;
        std::map<juce::String, Parameter*> parameterMap;

        float getCpuLoad();
        juce::AudioProcessorParameter* getBypassParameter() const override;

    protected:
        virtual void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {}

        juce::SharedResourcePointer<Resources> resources;
        ValueData valueData;

        bool firstBufferFlag {true};

        double getBPM();
        std::atomic<double> defaultBPM {120};
        std::atomic<double> lastBPM{120};
        std::atomic<double> lastSampleRate {48000};
        std::atomic<bool> lastPlaying{false};
        juce::ListenerList<BPMListener> bpmListeners;

        std::unique_ptr<Preset> nextPreset;

        void updateTrackProperties(const juce::AudioProcessor::TrackProperties& newProperties) override;
        juce::AudioProcessor::TrackProperties trackProperties;
        juce::ListenerList<PresetListener> presetListeners;
        juce::AudioPlayHead* playhead {nullptr};
        juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo;

        ModMatrix modMatrix;
        juce::Array<Parameter*> allParameters;
        juce::OwnedArray<Parameter> internalParameters;
        juce::ListenerList<ParameterListener> parameterListeners;

        juce::SmoothedValue<float> mixGain {0};
        juce::SmoothedValue<float> bypassGain {0};
        const int MAX_DELAY_LINES_SAMPLES_DURATION = 48000 * 4;
        juce::dsp::DelayLine<float> dryBufferLatencyCompensationLine { MAX_DELAY_LINES_SAMPLES_DURATION };

        juce::AudioProcessLoadMeasurer measurer;
        std::atomic<float> cpuLoad;

        void emitMessage(const std::string& type, choc::value::Value data);
        juce::ListenerList<MessageListener> messageListeners;

    private:
        void audioProcessorChanged(AudioProcessor *processor, const juce::AudioProcessorListener::ChangeDetails &details) override;
        void audioProcessorParameterChanged(AudioProcessor *processor, int parameterIndex, float newValue) override;
    };
}