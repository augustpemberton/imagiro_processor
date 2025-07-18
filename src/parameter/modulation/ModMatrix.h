// // Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModID.h"
#include "../../dsp/EnvelopeFollower.h"
#include "SerializedMatrix.h"
#include <stack>

#include "MultichannelValue.h"

#if !defined(MAX_MOD_VOICES)
    #define MAX_MOD_VOICES 128
#endif

#define MAX_MOD_TARGETS 4096

namespace imagiro {
    class ModMatrix {
    public:
        struct Listener {
            virtual ~Listener() = default;

            virtual void OnMatrixUpdated() {}
            virtual void OnRecentVoiceUpdated(size_t) {}
            virtual void OnTargetValueUpdated(const TargetID& targetID) {}
            virtual void OnSourceValueUpdated(const SourceID& sourceID) {}

            virtual void OnMatrixDestroyed(ModMatrix& m) {}
        };

        void addListener(Listener* l) { listeners.add(l); }
        void removeListener(Listener* l) { listeners.remove(l); }

        enum class SourceType {
            Note = 0,
            LFO = 1,
            Env = 2,
            Misc = 3
        };

        ModMatrix();
        ~ModMatrix();

        SourceID registerSource(std::string name = "", SourceType type = SourceType::Misc, bool isBipolar = false);
        void updateSource(SourceID id, const std::string &name = "", SourceType type = SourceType::Misc, bool isBipolar = false);
        TargetID registerTarget(std::string name = "");


        void removeSource(const SourceID& id) { sourcesToDelete.enqueue(id); }
        void removeTarget(const TargetID& id) { targetsToDelete.enqueue(id); }

        struct SourceValue {
            std::string name;
            bool bipolar;
            SourceType type;
            MultichannelValue<MAX_MOD_VOICES> value;

            choc::value::Value getState() const {
                auto v = choc::value::createObject("SourceValue");
                v.addMember("bipolar", bipolar);
                v.addMember("type", static_cast<int>(type));
                v.addMember("name", name);
                // v.addMember("values", value.getState());
                return v;
            }
        };

        struct TargetValue {
            std::string name;
            MultichannelValue<MAX_MOD_VOICES> value;

            choc::value::Value getState() const {
                auto v = choc::value::createObject("TargetValue");
                v.addMember("name", name);
                // v.addMember("values", value.getState());
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

        void setGlobalSourceValue(const SourceID& sourceID, float value);
        void setVoiceSourceValue(const SourceID& sourceID, size_t voiceIndex, float value);
        void resetSourceValue(const SourceID& sourceID);

        void prepareToPlay(double sampleRate, int maxSamplesPerBlock);
        void calculateTargetValues(int numSamples = 1);
        FixedHashSet<size_t, MAX_MOD_VOICES> getAlteredTargetVoices(const TargetID& targetID);

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
            // NOTE: commented this out for now as i don't think its desirable behaviour.

            std::erase_if(noteOnStack, [&, index](const size_t e) {
                return e == index;
            });
            auto oldRecentVoice = mostRecentVoiceIndex.load();
            mostRecentVoiceIndex = noteOnStack.back();
            if (oldRecentVoice != mostRecentVoiceIndex) listeners.call(&Listener::OnRecentVoiceUpdated, mostRecentVoiceIndex);
        }

        std::unordered_map<SourceID, std::shared_ptr<SourceValue>>& getSourceValues() { return sourceValues; }
        std::unordered_map<TargetID, std::shared_ptr<TargetValue>>& getTargetValues() { return targetValues; }

        const size_t getMostRecentVoiceIndex() const { return mostRecentVoiceIndex; }
        void processMatrixUpdates();

    private:
        std::atomic<size_t> mostRecentVoiceIndex {0};
        juce::LightweightListenerList<Listener> listeners;
        double sampleRate {48000};

        std::list<size_t> noteOnStack;

        MatrixType matrix{};

        std::unordered_map<SourceID, std::shared_ptr<SourceValue>> sourceValues {};
        std::unordered_map<TargetID, std::shared_ptr<TargetValue>> targetValues {};

        FixedHashSet<SourceID, MAX_MOD_TARGETS> updatedSourcesSinceLastCalculate {};
        FixedHashSet<SourceID, MAX_MOD_TARGETS> updatedTargetsSinceLastCalculate {};

        void matrixUpdated();

        moodycamel::ReaderWriterQueue<SourceID> sourcesToDelete {48};
        moodycamel::ReaderWriterQueue<SourceID> targetsToDelete {48};

        moodycamel::ReaderWriterQueue<std::shared_ptr<SourceValue>> sourcesToDeallocate {48};
        moodycamel::ReaderWriterQueue<std::shared_ptr<TargetValue>> targetsToDeallocate {48};

        moodycamel::ReaderWriterQueue<std::pair<SourceID, std::shared_ptr<SourceValue>>> newSourcesQueue {48};
        moodycamel::ReaderWriterQueue<std::pair<TargetID, std::shared_ptr<TargetValue>>> newTargetsQueue {48};

        std::atomic<bool> recacheSerializedMatrixFlag {false};
        SerializedMatrix cachedSerializedMatrix;

        FixedHashSet<TargetID, MAX_MOD_TARGETS> updatedTargets;

        int nextSourceID {0};
        int nextTargetID {0};
    };
}
