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
            virtual void OnRecentVoiceUpdated(size_t) {}

            virtual void OnConnectionAdded(const SourceID& source, const TargetID& target) {}
            virtual void OnConnectionUpdated(const SourceID& source, const TargetID& target) {}
            virtual void OnConnectionRemoved(const SourceID& source, const TargetID& target) {}

            virtual void OnTargetValueUpdated(const TargetID& targetID, const int voiceIndex = -1) {}
            virtual void OnTargetValueAdded(const TargetID& targetID) {}
            virtual void OnTargetValueRemoved(const TargetID& targetID) {}
            virtual void OnTargetValueReset(const TargetID& targetID) {}

            virtual void OnSourceValueUpdated(const SourceID& sourceID, const int voiceIndex = -1) {}
            virtual void OnSourceValueAdded(const SourceID& sourceID) {}
            virtual void OnSourceValueRemoved(const SourceID& sourceID) {}
            virtual void OnSourceValueReset(const SourceID& sourceID) {}

            virtual void OnMatrixDestroyed(ModMatrix& m) {}
        };

        void addListener(Listener* l) { listeners.add(l); }
        void removeListener(Listener* l) { listeners.remove(l); }

        ModMatrix();
        ~ModMatrix();

        SourceID registerSource(std::string name = "", bool bipolar = false);
        void updateSource(SourceID id, const std::string &name = "", bool bipolar = false);
        TargetID registerTarget(std::string name = "");

        void removeSource(const SourceID& id) { sourcesToDelete.enqueue(id); }
        void removeTarget(const TargetID& id) { targetsToDelete.enqueue(id); }

        struct SourceValue {
            std::string name;
            bool bipolar;
            MultichannelValue<MAX_MOD_VOICES> value;

            choc::value::Value getState() const {
                auto v = choc::value::createObject("SourceValue");
                v.addMember("name", name);
                v.addMember("bipolar", bipolar);
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
                bool bipolar {false};
                float attackMS {0};
                float releaseMS {0};
            };

            Connection(double sr = 48000, Settings s = {0, false, 10, 10})
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

        void queueConnection(const SourceID& sourceID, const TargetID& targetID, Connection::Settings connectionSettings);
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
        juce::ListenerList<Listener> listeners;
        double sampleRate {48000};

        std::list<size_t> noteOnStack;

        MatrixType matrix{};

        std::unordered_map<SourceID, std::shared_ptr<SourceValue>> sourceValues {};
        std::unordered_map<TargetID, std::shared_ptr<TargetValue>> targetValues {};

        FixedHashSet<SourceID, MAX_MOD_TARGETS> updatedSourcesSinceLastCalculate {};
        FixedHashSet<SourceID, MAX_MOD_TARGETS> updatedTargetsSinceLastCalculate {};

        moodycamel::ReaderWriterQueue<SourceID> sourcesToDelete {48};
        moodycamel::ReaderWriterQueue<SourceID> targetsToDelete {48};

        moodycamel::ReaderWriterQueue<std::shared_ptr<SourceValue>> sourcesToDeallocate {48};
        moodycamel::ReaderWriterQueue<std::shared_ptr<TargetValue>> targetsToDeallocate {48};

        moodycamel::ReaderWriterQueue<std::pair<SourceID, std::shared_ptr<SourceValue>>> newSourcesQueue {48};
        moodycamel::ReaderWriterQueue<std::pair<TargetID, std::shared_ptr<TargetValue>>> newTargetsQueue {48};

        struct ConnectionDefinition {
            SourceID sourceID;
            TargetID targetID;
            Connection::Settings connectionSettings;
        };

        moodycamel::ReaderWriterQueue<ConnectionDefinition> updatedConnectionsQueue {48};

        std::atomic<bool> recacheSerializedMatrixFlag {false};
        SerializedMatrix cachedSerializedMatrix;

        FixedHashSet<TargetID, MAX_MOD_TARGETS> updatedTargets {};

        int nextSourceID {0};
        int nextTargetID {0};
    };
}
