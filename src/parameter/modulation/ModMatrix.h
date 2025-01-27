// // Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModID.h"

namespace imagiro {

    class ModMatrix {
    public:

        SourceID registerSource() { return SourceID{nextSourceID++}; }
        TargetID registerTarget() { return TargetID{nextTargetID++}; }

        struct ConnectionInfo {
            float depth;
        };

        void setConnectionInfo(SourceID sourceID, TargetID targetID, ConnectionInfo connection);
        void removeConnectionInfo(SourceID sourceID, TargetID targetID);

        float getModulatedValue(TargetID targetID);
        void calculateTargetValues();

        void setSourceValue(SourceID sourceID, float value);

    private:
        std::unordered_map<std::pair<SourceID, TargetID>, ConnectionInfo> matrix{};

        uint nextSourceID = 0;
        uint nextTargetID = 0;

        std::unordered_map<SourceID, float> sourceValues{};
        std::unordered_map<TargetID, float> targetValues{};
    };
}