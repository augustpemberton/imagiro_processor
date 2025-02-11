// // Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModID.h"
#include "../../dsp/EnvelopeFollower.h"
#include "SerializedMatrix.h"

#if defined(MAX_VOICES)
    #define MAX_MOD_VOICES MAX_VOICES
#else if !defined(MAX_MOD_VOICES)
    #define MAX_MOD_VOICES 128
#endif

namespace imagiro {
    class ModMatrix {
    public:
        struct Listener {
            virtual void OnMatrixUpdated() {}
            virtual void OnRecentVoiceUpdated(size_t voiceIndex) {}
        };

        void addListener(Listener* l) { listeners.add(l); }
        void removeListener(Listener* l) { listeners.remove(l); }

        SourceID registerSource(std::string name = "");
        TargetID registerTarget(std::string name = "");

        struct SourceValue {
            float globalModValue {0};
            std::array<float, MAX_MOD_VOICES> voiceModValues {};
            std::set<int> alteredVoiceValues {};

            choc::value::Value getValue() const {
                auto v = choc::value::createObject("SourceValue");
                v.addMember("global", globalModValue);
                for (const auto index : alteredVoiceValues) {
                    v.addMember(std::to_string(index), voiceModValues[index]);
                }
                return v;
            }
        };

        struct TargetValue {
            float globalModValue {0};
            std::array<float, MAX_MOD_VOICES> voiceModValues {};
            std::set<int> alteredVoiceValues {};

            choc::value::Value getValue() const {
                auto v = choc::value::createObject("TargetValue");
                v.addMember("global", globalModValue);
                for (const auto index : alteredVoiceValues) {
                    v.addMember(std::to_string(index), voiceModValues[index]);
                }
                return v;
            }
        };

        class Connection {
        public:
            struct Settings {
                float depth {0};
                float attackMS {10};
                float releaseMS {10};
            };

            Connection(double sr = 44100, Settings s = {0, 10, 10})
                : sampleRate(sr), settings(s)
            {
                setSettings(s);
                setSampleRate(sampleRate);
            }

            Connection(const Connection& other) : sampleRate(other.sampleRate) {
                setSettings(other.settings);
                setSampleRate(other.sampleRate);
            }

            void setSampleRate(double sr) {
                sampleRate = sr;

                globalValueEnvelopeFollower.setSampleRate(sampleRate);
                for (auto& e : voiceValueEnvelopeFollowers) {
                    e.setSampleRate(sampleRate);
                }
            }

            void setSettings(Settings s) {
                settings = s;
                globalValueEnvelopeFollower.setAttackMs(settings.attackMS);
                globalValueEnvelopeFollower.setReleaseMs(settings.releaseMS);
                globalValueEnvelopeFollower.setSampleRate(sampleRate);

                for (auto& e : voiceValueEnvelopeFollowers) {
                    e.setAttackMs(settings.attackMS);
                    e.setReleaseMs(settings.releaseMS);
                    e.setSampleRate(sampleRate);
                }
            }

            EnvelopeFollower<float>& getGlobalEnvelopeFollower() { return globalValueEnvelopeFollower; }
            EnvelopeFollower<float>& getVoiceEnvelopeFollower(size_t index) { return voiceValueEnvelopeFollowers[index]; }
            Settings getSettings() const { return settings; }
        private:
            double sampleRate;
            Settings settings;
            EnvelopeFollower<float> globalValueEnvelopeFollower;
            std::array<EnvelopeFollower<float>, MAX_MOD_VOICES> voiceValueEnvelopeFollowers;
        };

        void setConnection(SourceID sourceID, TargetID targetID, Connection::Settings connectionSettings);
        void removeConnection(SourceID sourceID, TargetID targetID);

        float getModulatedValue(TargetID targetID, int voiceIndex = -1);
        int getNumModSources(TargetID targetID);

        void setGlobalSourceValue(SourceID sourceID, float value);
        void setVoiceSourceValue(SourceID sourceID, size_t voiceIndex, float value);

        void prepareToPlay(double sampleRate, int maxSamplesPerBlock);
        void calculateTargetValues(int numSamples = 1);
        std::set<int> getAlteredTargetVoices(TargetID targetID);

        auto& getMatrix() { return matrix; }
        auto& getSourceNames() { return sourceNames; }
        auto& getTargetNames() { return targetNames; }

        SerializedMatrix getSerializedMatrix();
        void loadSerializedMatrix(const SerializedMatrix& m);

        using MatrixType = std::unordered_map<std::pair<SourceID, TargetID>, ModMatrix::Connection>;

        void setMostRecentVoiceIndex(size_t index) {
            mostRecentVoiceIndex = index;
            listeners.call(&Listener::OnRecentVoiceUpdated, index);
        }

        std::unordered_map<SourceID, SourceValue>& getSourceValues() { return sourceValues; };
        std::unordered_map<TargetID, TargetValue>& getTargetValues() { return targetValues; };

    private:
        size_t mostRecentVoiceIndex {0};
        juce::ListenerList<Listener> listeners;
        double sampleRate {44100};

        MatrixType matrix{};

        unsigned int nextSourceID = 0;
        unsigned int nextTargetID = 0;

        std::unordered_map<SourceID, SourceValue> sourceValues {};
        std::unordered_map<TargetID, TargetValue> targetValues {};

        std::unordered_map<SourceID, std::string> sourceNames;
        std::unordered_map<TargetID, std::string> targetNames;

        std::unordered_map<TargetID, int> numModSources {};
    };
}
