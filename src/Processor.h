//
// Created by August Pemberton on 12/08/2022.
//

#pragma once
#include <juce_core/juce_core.h>
#include <imagiro_util/imagiro_util.h>
#include "ProcessorBase.h"
#include "parameter/Parameter.h"
#include "preset/Preset.h"
#include "config/AuthorizationManager.h"
#include "config/VersionManager.h"
#include "imagiro_processor/src/preset/FileBackedPreset.h"
#include "imagiro_processor/src/config/Resources.h"
#include "imagiro_processor/src/parameter/ParameterLoader.h"

class Preset;

namespace imagiro {
    class Processor : public ProcessorBase, public Parameter::Listener {
    public:
        Processor(juce::String parametersYAMLString,
                  const juce::String& currentVersion = "1.0.0",
                  const juce::String& productSlug = "");
        Processor(const juce::AudioProcessor::BusesProperties& ioLayouts,
                  juce::String parametersYAMLString,
                  const juce::String& currentVersion = "1.0.0",
                  const juce::String& productSlug = "");

        ~Processor() override;

        // =================================================================
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
        Parameter* getParameter (const juce::String& uid);
        const juce::Array<Parameter*>& getPluginParameters();

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
        virtual void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {}

        void parameterChanged(imagiro::Parameter *param) override;
        void configChanged(imagiro::Parameter *param) override;
        std::map<juce::String, Parameter*> parameterMap;

        AuthorizationManager& getAuthManager() { return authManager; }
        VersionManager& getVersionManager() { return versionManager; }

        float getCpuLoad();

    protected:
        AuthorizationManager authManager;
        VersionManager versionManager;
        juce::SharedResourcePointer<Resources> resources;

        double getBPM();
        std::atomic<double> lastBPM{120};
        std::atomic<double> lastSampleRate {44100};
        std::atomic<bool> lastPlaying{false};
        juce::ListenerList<BPMListener> bpmListeners;

        std::unique_ptr<Preset> nextPreset;

        void updateTrackProperties(const juce::AudioProcessor::TrackProperties& newProperties) override;
        juce::AudioProcessor::TrackProperties trackProperties;
        juce::ListenerList<PresetListener> presetListeners;
        juce::AudioPlayHead* playhead {nullptr};
        juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo;

        juce::Array<Parameter*> allParameters;
        juce::OwnedArray<Parameter> internalParameters;

        juce::SmoothedValue<float> bypassGain;
        juce::AudioSampleBuffer dryBuffer;

        juce::AudioProcessLoadMeasurer measurer;
        std::atomic<float> cpuLoad;

        ParameterLoader paramLoader;
    };
}
