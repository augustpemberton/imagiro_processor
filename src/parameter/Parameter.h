//
// Created by August Pemberton on 12/09/2022.
//

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterConfig.h"
#include "ParameterHelpers.h"
#include "choc/containers/choc_Value.h"

namespace imagiro {
    class Processor;
    class Parameter;
    class ParameterListener
    {
    public:
        virtual ~ParameterListener() = default;
        virtual void parameterChanged (Parameter* param) {}
        virtual void parameterChangedSync (Parameter* param) {}
        virtual void rangeChanged(Parameter* param) {}
        virtual void gestureStarted(Parameter* param) {}
        virtual void gestureEnded(Parameter* param) {}
    };

    class ModulationMatrix;
    class Parameter : public juce::RangedAudioParameter, protected juce::Timer {
    public:
        using Listener = ParameterListener;
        Parameter(juce::String uid, juce::String name,
                  ParameterConfig config, bool internal = false,
                  bool automatable = true, int versionHint=1);

        Parameter(juce::String uid, juce::String name,
                  std::vector<ParameterConfig> config, bool internal = false,
                  bool automatable = true, int versionHint=1);

        ~Parameter() override;

        void setInternal (bool i)           { internal = i;     }
        bool isInternal() const             { return internal;  }
        juce::String getUID()               { return uid;       }

        virtual void reset() {}
        virtual void prepareToPlay (double sampleRate, int samplesPerBlock);

        //==============================================================================
        void setModIndex (int i)            { modIndex = i;     }
        int getModIndex() const                   { return modIndex;  }
        void setModMatrix (ModulationMatrix* m)    { modMatrix = m;    }
        ModulationMatrix* getModMatrix()           { return modMatrix; }

        //==============================================================================
        bool isToggle();

        float getModVal(bool useConversion = true);
        virtual float getVal(int samplesToAdvance = 0);
        float getUserValue() const;
        int getUserValueInt() const;
        bool getUserValueBool() const;
        float getUserDefaultValue() const;

        virtual void setUserValue (float v);
        virtual void setUserValueNotifingHost (float f, bool force = false);
        void setUserValueAsUserAction (float f);
        juce::String getUserValueText() const;
        juce::String userValueToText (float val);
        DisplayValue getDisplayValue() const;
        DisplayValue getDisplayValueForUserValue(float userValue) const;

        void updateCache() { cachedValue = getModVal(); }
        float cached() const { return cachedValue; }

        //==============================================================================
        void beginUserAction();
        void endUserAction();

        juce::NormalisableRange<float> getUserRange() const;
        float convertTo0to1 (float v) const;
        float convertFrom0to1 (float v) const;

        //==============================================================================

        void addListener (Listener* listener);
        void removeListener (Listener* listener);

        //==============================================================================
        struct ParamState {
            juce::String uid;
            float value;
            juce::String config {0};
            bool locked;

            bool operator==(const ParamState& other) {
                return uid == other.uid &&
                       abs(value - other.value) < 0.0001f &&
                       config == other.config &&
                       locked == other.locked;
            }

            bool operator!=(const ParamState& other) {
                return !operator==(other);
            }

            choc::value::Value getState() {
                auto state = choc::value::createObject("ParamState");
                state.addMember("uid", uid.toStdString());
                state.addMember("value", value);
                state.addMember("config", config.toStdString());
                state.addMember("locked", locked);
                return state;
            }

            static ParamState fromState(choc::value::ValueView& state) {
                return {
                    state["uid"].getWithDefault(""),
                    state["value"].getWithDefault(0.f),
                    state["config"].getWithDefault(""),
                    state["locked"].getWithDefault(false),
                };
            }
        };

        ParamState getState();
        void setState(const ParamState& state);

        //==============================================================================
        juce::String getParameterID() const override    { return uid; }

        float getValue() const override;
        bool getBoolValue() const                   { return getValue() != 0.0f; }

        void setValue (float newValue) override;
        float getDefaultValue() const override;

        juce::String getName (int maximumStringLength) const override;
        juce::String getLabel() const override;

        int getNumSteps() const override;
        juce::String getText (float value, int /*maximumStringLength*/) const override;
        float getValueForText (const juce::String& text) const override;

        bool isOrientationInverted() const override;
        bool isAutomatable() const override;
        bool isMetaParameter() const override;

        const juce::NormalisableRange<float>& getNormalisableRange() const override;

        std::vector<ParameterConfig> configs;
        int configIndex {0};
        const ParameterConfig* getConfig() const;
        ParameterConfig* getConfig();

        int getConfigIndex();
        void setConfig(int index);

        void setLocked(bool locked);
        bool isLocked() const;

        void generateSmoothedValueBlock(int samples);
        float getSmoothedValue(int blockIndex);
        float getSmoothedUserValue(int blockIndex);

    protected:
        bool internal {false};
        ModulationMatrix* modMatrix = nullptr;
        int modIndex = -1;

        std::atomic<bool> sendUpdateFlag {false};
        void timerCallback() override;
        virtual void valueChanged();

        juce::String uid;
        juce::String name;
        juce::String suffix;

        float value01 {0};
        float cachedValue {0};

        int currentUserActionsCount = 0;

        juce::ListenerList<Listener> listeners;

        bool locked {false};

        juce::SmoothedValue<float> valueSmoother;
        juce::AudioSampleBuffer smoothedValueBuffer;
    };

}

