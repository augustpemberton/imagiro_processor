// // Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModID.h"
#include <variant>

namespace imagiro {
    class ModMatrix {
    public:
        enum class ModulationType { Global, PerVoice };
        static constexpr int MAX_VOICES = 128;

        SourceID registerSource(ModulationType type);
        TargetID registerTarget(ModulationType type);

        struct ConnectionInfo {
            float depth;
        };

        void setConnectionInfo(SourceID sourceID, TargetID targetID, ConnectionInfo connection);
        void removeConnectionInfo(SourceID sourceID, TargetID targetID);

        float getModulatedValue(TargetID targetID);
        float getModulatedValue(TargetID targetID, size_t voiceIndex);
        void calculateTargetValues();

        void setSourceValue(SourceID sourceID, float value);
        void setSourceValue(SourceID sourceID, size_t voiceIndex, float value);

        int getNumModSources(TargetID targetID);

    private:
        static std::variant<float, std::array<float, MAX_VOICES>> getDefaultVariantValue(ModulationType type) {
            if (type == ModulationType::Global) return 0.f;
            else return std::array<float, MAX_VOICES>();
        }

        struct SourceValue {
            ModulationType type;
            std::variant<float, std::array<float, MAX_VOICES>> value;
        };

        struct TargetValue {
            ModulationType type;
            std::variant<float, std::array<float, MAX_VOICES>> value;
        };

        std::unordered_map<std::pair<SourceID, TargetID>, ConnectionInfo> matrix{};

        uint nextSourceID = 0;
        uint nextTargetID = 0;

        std::unordered_map<SourceID, SourceValue> sourceValues {};
        std::unordered_map<TargetID, TargetValue> targetValues {};
        std::unordered_map<TargetID, int> numModSources {};
    };
}
