// // Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModID.h"
#include <variant>

namespace imagiro {
    class ModMatrix {
    public:
        static constexpr int MAX_VOICES = 128;

        struct Listener {
            virtual void OnMatrixUpdated() {}
        };

        void addListener(Listener* l) { listeners.add(l); }
        void removeListener(Listener* l) { listeners.remove(l); }

        SourceID registerSource(std::string name = "");
        TargetID registerTarget(std::string name = "");

        struct ConnectionInfo {
            float depth;
        };

        void setConnectionInfo(SourceID sourceID, TargetID targetID, ConnectionInfo connection);
        void removeConnectionInfo(SourceID sourceID, TargetID targetID);

        float getModulatedValue(TargetID targetID, int voiceIndex = -1);
        int getNumModSources(TargetID targetID);

        void setGlobalSourceValue(SourceID sourceID, float value);
        void setVoiceSourceValue(SourceID sourceID, size_t voiceIndex, float value);

        void calculateTargetValues();

        auto getMatrix() { return matrix; }
        auto& getSourceNames() { return sourceNames; }
        auto& getTargetNames() { return targetNames; }

    private:
        juce::ListenerList<Listener> listeners;
        struct SourceValue {
            float globalModValue {0};
            std::array<float, MAX_VOICES> voiceModValues;
            std::set<int> alteredVoiceValues;
        };

        struct TargetValue {
            float globalModValue {0};
            std::array<float, MAX_VOICES> voiceModValues;
        };

        std::unordered_map<std::pair<SourceID, TargetID>, ConnectionInfo> matrix{};

        uint nextSourceID = 0;
        uint nextTargetID = 0;

        std::unordered_map<SourceID, SourceValue> sourceValues {};
        std::unordered_map<TargetID, TargetValue> targetValues {};

        std::unordered_map<SourceID, std::string> sourceNames;
        std::unordered_map<TargetID, std::string> targetNames;

        std::unordered_map<TargetID, int> numModSources {};
    };
}
