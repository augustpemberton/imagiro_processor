// // Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModID.h"
#include "../../dsp/EnvelopeFollower.h"
#include "SerializedMatrix.h"
#include <stack>

#if defined(MAX_VOICES)
    #define MAX_MOD_VOICES MAX_VOICES
#elif !defined(MAX_MOD_VOICES)
    #define MAX_MOD_VOICES 128
#endif

namespace imagiro {
    class ModMatrix {
    public:
        struct Listener {
            virtual void OnMatrixUpdated() {}
            virtual void OnRecentVoiceUpdated(size_t) {}
        };

        void addListener(Listener* l) { listeners.add(l); }
        void removeListener(Listener* l) { listeners.remove(l); }

        enum class SourceType {
            Note = 0,
            LFO = 1,
            Env = 2,
            Misc = 3
        };

        void registerSource(const SourceID& id, std::string name = "",
                            SourceType type = SourceType::Misc, bool isBipolar = false);
        void registerTarget(const TargetID& id, std::string name = "");

        struct SourceValue {
            std::string name;
            float globalModValue {0};
            std::array<float, MAX_MOD_VOICES> voiceModValues {};
            std::set<int> alteredVoiceValues {};
            bool bipolar;
            SourceType type;

            choc::value::Value getValue() const {
                auto v = choc::value::createObject("SourceValue");
                v.addMember("bipolar", bipolar);
                v.addMember("type", static_cast<int>(type));
                v.addMember("name", name);

                auto values = choc::value::createObject("Values");
                values.addMember("global", globalModValue);
                for (const auto index : alteredVoiceValues) {
                    values.addMember(std::to_string(index), voiceModValues[index]);
                }
                v.addMember("values", values);

                return v;
            }
        };

        struct TargetValue {
            std::string name;
            float globalModValue {0};
            std::array<float, MAX_MOD_VOICES> voiceModValues {};
            std::set<int> alteredVoiceValues {};

            choc::value::Value getValue() const {
                auto v = choc::value::createObject("TargetValue");
                v.addMember("name", name);

                auto values = choc::value::createObject("Values");
                values.addMember("global", globalModValue);
                for (const auto index : alteredVoiceValues) {
                    values.addMember(std::to_string(index), voiceModValues[index]);
                }
                v.addMember("values", values);

                return v;
            }
        };

        class Connection {
        public:
            struct Settings {
                float depth {0};
                float attackMS {0};
                float releaseMS {0};
                bool bipolar {false};
            };

            Connection(double sr = 48000, Settings s = {0, 10, 10, false})
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

        void setConnection(const SourceID& sourceID, const TargetID& targetID, Connection::Settings connectionSettings);
        void removeConnection(const SourceID& sourceID, TargetID targetID);

        float getModulatedValue(const TargetID& targetID, int voiceIndex = -1);
        int getNumModSources(const TargetID& targetID);

        void setGlobalSourceValue(const SourceID& sourceID, float value);
        void setVoiceSourceValue(const SourceID& sourceID, size_t voiceIndex, float value);

        void prepareToPlay(double sampleRate, int maxSamplesPerBlock);
        void calculateTargetValues(int numSamples = 1);
        std::set<int> getAlteredTargetVoices(const TargetID& targetID);

        auto& getMatrix() { return matrix; }

        SerializedMatrix getSerializedMatrix();
        void loadSerializedMatrix(const SerializedMatrix& m);

        using MatrixType = std::unordered_map<std::pair<SourceID, TargetID>, ModMatrix::Connection>;

        void setVoiceOn(size_t index) {
            mostRecentVoiceIndex = index;
            listeners.call(&Listener::OnRecentVoiceUpdated, index);
            noteOnStack.push_back(index);
        }

        void setVoiceOff(size_t index) {
            std::erase_if(noteOnStack, [&, index](const size_t e) {
                return e == index;
            });
            auto oldRecentVoice = mostRecentVoiceIndex.load();
            mostRecentVoiceIndex = noteOnStack.back();
            if (oldRecentVoice != mostRecentVoiceIndex) listeners.call(&Listener::OnRecentVoiceUpdated, mostRecentVoiceIndex);
        }

        std::unordered_map<SourceID, SourceValue>& getSourceValues() { return sourceValues; }
        std::unordered_map<TargetID, TargetValue>& getTargetValues() { return targetValues; }

        size_t getMostRecentVoiceIndex() { return mostRecentVoiceIndex; }

    private:
        std::atomic<size_t> mostRecentVoiceIndex {0};
        juce::ListenerList<Listener> listeners;
        double sampleRate {48000};

        std::list<size_t> noteOnStack;

        MatrixType matrix{};

        std::unordered_map<SourceID, SourceValue> sourceValues {};
        std::unordered_map<TargetID, TargetValue> targetValues {};

        std::unordered_map<TargetID, int> numModSources {};
    };
}
