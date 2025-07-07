// // Created by August Pemberton on 12/09/2022.
//

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterConfig.h"
#include "choc/containers/choc_Value.h"
#include "modulation/ModTarget.h"
#include "modulation/ModTarget.h"

namespace imagiro {
    class Processor;

    class Parameter : protected juce::RangedAudioParameter, juce::Timer, ModTarget::Listener {
    public:
        class Listener {
        public:
            virtual ~Listener() = default;

            virtual void parameterChanged (Parameter* ) {}
            virtual void parameterChangedSync (Parameter* ) {}
            virtual void configChanged(Parameter* ) {}
            virtual void configChangedSync(Parameter* ) {}
            virtual void gestureStarted(Parameter* ) {}
            virtual void gestureStartedSync(Parameter* ) {}
            virtual void gestureEnded(Parameter* ) {}
            virtual void gestureEndedSync(Parameter* ) {}
            virtual void lockChanged(Parameter* ) {}
        };

        Parameter(std::string uid, std::string name,
                  std::vector<ParameterConfig> config,
                  bool internal = false,
                  bool isMetaParam = false,
                  bool automatable = true,
                  float jitterAmount=0.f,
                  int versionHint=1);

        ~Parameter() override;

        void setModMatrix(ModMatrix& m);

        RangedAudioParameter* asJUCEParameter() { return this; }

        void setInternal (bool i)          { internal = i;     }
        bool isInternal() const            { return internal;  }
        bool isMeta() const                { return juce::RangedAudioParameter::isMetaParameter();  }
        bool isAutomatable() const         { return juce::RangedAudioParameter::isAutomatable();  }
        std::string getUID()               { return uid;       }

        virtual void reset() {}
        virtual void prepareToPlay (double sampleRate, int samplesPerBlock);

        //==============================================================================
        bool isToggle();

        /*!
         * @return the user value with an optional conversion function applied.
         * note - does not map back to value01
         */
        float getProcessorValue(int voiceIndex = -1) const;

        /*!
         * @return the user value - what the user sees
         */
        float getUserValue() const;


        /*!
         * @return the internal 0-1 value
         */
        float getValue() const override;

        bool getBoolValue() const;

        float getModValue(int voiceIndex = -1) const;
        float getModUserValue(int voiceIndex = -1) const;

        float getDefaultValue() const override;
        float getUserDefaultValue() const;

        void setJitterAmount(float amount) {
            jitterAmount = amount;
        }
        virtual float getJitterAmount() const { return jitterAmount; }

        virtual void setValue (float newValue) override;
        void setUserValue (float v);
        virtual void setValueAndNotifyHost (float f, bool forceUpdate = false);
        void setUserValueAndNotifyHost (float f, bool forceUpdate = false);
        void setUserValueAsUserAction (float f);

        virtual std::string getUserValueText() const;
        std::string userValueToText (float val);
        DisplayValue getDisplayValue() const;
        DisplayValue getDisplayValueForUserValue(float userValue) const;

        void updateCachedUserValue() { cachedUserValue = getUserValue(); }
        float getCachedUserValue() const { return cachedUserValue; }

        virtual void callValueChangedListeners();

        //==============================================================================

        void beginUserAction();
        void endUserAction();
        float convertTo0to1 (float v) const;
        float convertFrom0to1 (float v, bool snapToLegalValue = true) const;

        //==============================================================================

        void addListener (Listener* listener);
        void removeListener (Listener* listener);

        //==============================================================================
        struct ParamState {
            std::string uid;
            float value;
            std::string config;
            float jitter {0};
            bool locked {false};

            bool operator==(const ParamState& other) const {
                return uid == other.uid &&
                       abs(value - other.value) < 0.0001f &&
                       config == other.config &&
                        almostEqual(jitter, other.jitter) &&
                       locked == other.locked;
            }

            bool operator!=(const ParamState& other) const {
                return !operator==(other);
            }

            [[nodiscard]] choc::value::Value getStateCompressed(bool isDAWSaveState = false) const {
                auto state = choc::value::createEmptyArray();
                state.addArrayElement(uid);
                state.addArrayElement(value);
                state.addArrayElement(config);
                state.addArrayElement(jitter);
                if (isDAWSaveState) state.addArrayElement(locked);
                return state;
            }

            static ParamState fromStateCompressed(choc::value::ValueView& state) {
                auto s = ParamState {
                        state[0].getWithDefault(""),
                        state[1].getWithDefault(0.f),
                        state[2].getWithDefault(""),
                        state.size() > 3 ? state[3].getWithDefault(0.f) : 0.f,
                };

                if (state.size() > 4) {
                    s.locked = state[4].getWithDefault(false);
                }
                return s;
            }

            [[nodiscard]] choc::value::Value getState(bool isDAWSaveState = false) const {
                auto state = choc::value::createObject("ParamState");
                state.addMember("uid", uid);
                state.addMember("value", value);
                state.addMember("config", config);
                state.addMember("jitter", jitter);
                if (isDAWSaveState) state.addMember("locked", locked);
                return state;
            }

            static ParamState fromState(choc::value::ValueView& state) {
                if (state.isArray()) return fromStateCompressed(state);
                return {
                    state["uid"].getWithDefault(""),
                    state["value"].getWithDefault(0.f),
                    state["config"].getWithDefault(""),
                    state["jitter"].getWithDefault(0.f),
                    state["locked"].getWithDefault(false),
                };
            }
        };

        ParamState getState();
        void setState(const ParamState& state);

        //==============================================================================
        juce::String getParameterID() const override    { return uid; }

        void setName (const juce::String& n);
        juce::String getName (int maximumStringLength) const override;
        juce::String getLabel() const override;

        int getNumSteps() const override;
        bool isDiscrete() const override;
        juce::String getText (float value, int /*maximumStringLength*/) const override;
        float getValueForText (const juce::String& text) const override;

        bool isOrientationInverted() const override;

        const juce::NormalisableRange<float>& getNormalisableRange() const override;

        virtual ParameterConfig* getConfig();
        const virtual ParameterConfig* getConfig() const;
        virtual std::vector<ParameterConfig> getAllConfigs() const {
            return configs;
        }

        virtual int getConfigIndex();
        virtual void setConfig(int index);

        virtual void setLocked(bool locked);
        virtual bool isLocked() const;

        void startBlock(int samples);
        float getSmoothedValue(int blockIndex);
        void setSmoothTime(float seconds);
        juce::AudioSampleBuffer& getSmoothedProcessorValueBuffer();

        virtual ModTarget& getModTarget() { return modTarget; }
        void setModTarget(const ModTarget& target) { modTarget = target; }
        void OnTargetUpdated() override;

    protected:
        std::vector<ParameterConfig> configs;

        bool internal {false};
        int modIndex = -1;
        unsigned int configIndex {0};

        virtual void valueChanged();

        void timerCallback() override;

        std::string uid;
        std::string name;
        std::string suffix;

        std::atomic<float> value01 {0};
        float cachedUserValue {0};

        juce::LightweightListenerList<Listener> listeners;

        std::atomic<bool> locked {false};

        juce::SmoothedValue<float> valueSmoother;
        juce::AudioSampleBuffer smoothedValueBuffer;
        float smoothTimeSeconds {0.01f};
        double sampleRate;
        std::atomic<bool> smootherNeedsUpdate {false};
        int samplesThisBlock {0};
        bool hasGeneratedSmoothBufferThisBlock {false};
        void generateSmoothedValueBuffer(int samples);

        std::atomic<bool> asyncValueUpdateFlag;
        std::atomic<bool> asyncConfigChangedFlag;
        std::atomic<bool> asyncGestureStartUpdateFlag;
        std::atomic<bool> asyncGestureEndUpdateFlag;

        ModMatrix* modMatrix {nullptr};
        ModTarget modTarget;

        float jitterAmount {0};
    };
}
