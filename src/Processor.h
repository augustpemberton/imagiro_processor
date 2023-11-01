//
// Created by August Pemberton on 12/08/2022.
//

#pragma once
#include <juce_core/juce_core.h>
#include <imagiro_util/imagiro_util.h>
#include "ProcessorBase.h"
#include "Scale.h"
#include "parameter/Parameter.h"
#include "preset/Preset.h"
#include "config/Authorization.h"
#include "config/VersionManager.h"
#include "imagiro_processor/src/preset/FileBackedPreset.h"

class Preset;

namespace imagiro {
    class SmoothingType {
    public:
        enum Type {
            linear, eased
        };

        SmoothingType(float time_ = 0.1f, Type type_ = linear)
                : time(time_), type(type_) {}

        float time;
        Type type;
    };

    class Processor : public ProcessorBase, public Parameter::Listener {
    public:

        Processor(juce::String currentVersion = "1.0.0", juce::String productSlug = "");
        Processor(const juce::AudioProcessor::BusesProperties& ioLayouts,
                  juce::String currentVersion = "1.0.0", juce::String productSlug = "");

        ~Processor() override;

        struct BPMListener {
            virtual void bpmChanged(double newBPM) {}
            virtual void sampleRateChanged(double newRate) {}
            virtual void playChanged(bool isPlaying) {}
        };
        void addBPMListener(BPMListener* l) { bpmListeners.add(l); }
        void removeBPMListener(BPMListener* l) { bpmListeners.remove(l); }

        // =================================================================
        void reset() override;

        Parameter* addParam (std::unique_ptr<Parameter> p);

        using AudioProcessor::getParameter;
        Parameter* getParameter (const juce::String& uid);
        const juce::Array<Parameter*>& getPluginParameters();

        // =================================================================
        struct PresetListener {
            virtual void OnPresetChange(Preset& preset) {}
        };

        void addPresetListener(PresetListener *l);
        void removePresetListener(PresetListener *l);

        virtual void randomizeParameters();
        void queuePreset(FileBackedPreset preset, bool loadOnAudioThread = false);
        void queuePreset(Preset preset, bool loadOnAudioThread = false);
        virtual Preset createPreset(const juce::String &name, bool isDawPreset = false);

        void getStateInformation(juce::MemoryBlock &destData) override;
        void setStateInformation(const void *data, int sizeInBytes) override;

        std::optional<FileBackedPreset> lastLoadedPreset;
        bool showPresets{false};
        bool showFavoritePresetsOnly {false};

        // =================================================================
        double getLastSampleRate() const {return lastSampleRate; }

        double getSyncTimeSeconds(float proportionOfBeat);
        double getLastBPM() const;
        static double getNoteLengthSamples(double bpm, float proportionOfBeat, double sampleRate);
        double getNoteLengthSamples(float proportionOfBeat);
        static float getSamplesPerBeat(float bpm, double sampleRate);
        float getSamplesPerBeat();
        float getNotes(float seconds);
        float getNotesFromSamples(float samples);

        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override;
        virtual void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {};
        void parameterChanged(imagiro::Parameter *param) override;

        std::map<juce::String, Parameter*> parameterMap;
        juce::OwnedArray<Parameter> internalParameters;

        juce::ValueTree state;

        juce::Optional<juce::AudioPlayHead::PositionInfo>& getPosition() { return posInfo; }
        juce::AudioProcessor::TrackProperties getProperties() { return trackProperties; }

        void setScale(const juce::String& scaleID, juce::BigInteger state);
        Scale* getScale(juce::String scaleID);

        Authorization auth;
        VersionManager versionManager;

    protected:
        double lastSampleRate {44100};

        juce::AudioPlayHead* playhead {nullptr};
        juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo;

        double getBPM();
        std::atomic<double> lastBPM{120};
        std::atomic<bool> lastPlaying{false};
        juce::ListenerList<BPMListener> bpmListeners;

        std::unique_ptr<Preset> nextPreset;
        virtual void loadPreset(FileBackedPreset preset);
        virtual void loadPreset(Preset preset);

        void updateTrackProperties(const juce::AudioProcessor::TrackProperties& newProperties) override;
        juce::AudioProcessor::TrackProperties trackProperties;
        juce::ListenerList<PresetListener> presetListeners;

        std::map<juce::String, Scale> scales;
        choc::value::Value getScalesState();
        void loadScalesTree(const choc::value::ValueView& t);

        juce::Array<Parameter*> allParameters;

        juce::SmoothedValue<float> bypassGain;
        juce::AudioSampleBuffer dryBuffer;

    };
}