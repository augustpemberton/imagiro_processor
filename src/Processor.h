//
// Created by August Pemberton on 12/08/2022.
//

#pragma once
#include <juce_core/juce_core.h>
#include <imagiro-util/imagiro-util.h>
#include "ProcessorBase.h"
#include "./modulation/ModulationMatrix.h"
#include "parameter/Parameter.h"
#include "./Scale.h"

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

    class Processor : public ProcessorBase {
    public:

        Processor();
        Processor(const juce::AudioProcessor::BusesProperties& ioLayouts);

        struct BPMListener {
            virtual void bpmChanged(double newBPM) {}
            virtual void sampleRateChanged(double newRate) {}
            virtual void playChanged(bool isPlaying) {}
        };
        void addBPMListener(BPMListener* l) { bpmListeners.add(l); }
        void removeBPMListener(BPMListener* l) { bpmListeners.remove(l); }

        // =================================================================
        void reset() override;
        void addPluginParameter (Parameter* parameter);

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
        void queuePreset(Preset preset, bool loadOnAudioThread = false);
        virtual Preset createPreset(const juce::String &name, bool isDawPreset = false);

        void getStateInformation(juce::MemoryBlock &destData) override;
        void setStateInformation(const void *data, int sizeInBytes) override;

        std::unique_ptr<Preset> lastLoadedPreset;
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

        void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override;

        // =================================================================
        imagiro::ModulationMatrix& getModMatrix() { return modMatrix; }

    public:
        //juce::SharedResourcePointer<ImagiroLookAndFeel> lf;

        std::map<juce::String, Parameter*> parameterMap;
        juce::OwnedArray<Parameter> internalParameters;

        juce::ValueTree state;

        juce::Optional<juce::AudioPlayHead::PositionInfo>& getPosition() { return posInfo; }
        juce::AudioProcessor::TrackProperties getProperties() { return trackProperties; }

        void setScale(const juce::String& scaleID, juce::BigInteger state);
        Scale* getScale(juce::String scaleID);

    protected:
        imagiro::ModulationMatrix modMatrix;
        double lastSampleRate {44100};

        juce::AudioPlayHead* playhead {nullptr};
        juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo;

        double getBPM();
        std::atomic<double> lastBPM{120};
        std::atomic<bool> lastPlaying{false};
        juce::ListenerList<BPMListener> bpmListeners;

        std::unique_ptr<Preset> nextPreset;
        virtual void loadPreset(Preset preset);

        void updateTrackProperties(const juce::AudioProcessor::TrackProperties& newProperties) override;
        juce::AudioProcessor::TrackProperties trackProperties;
        juce::ListenerList<PresetListener> presetListeners;

        std::map<juce::String, Scale> scales;
        juce::ValueTree getScalesTree();
        void loadScalesTree(juce::ValueTree t);

        juce::Array<Parameter*> allParameters;

    };
}