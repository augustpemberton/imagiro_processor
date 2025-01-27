//
// Created by August Pemberton on 27/01/2025.
//

#include "ModMatrix.h"

namespace imagiro {
    void ModMatrix::removeConnectionInfo(SourceID sourceID, TargetID targetID) {
        matrix.erase({sourceID, targetID});
    }

    void ModMatrix::setConnectionInfo(SourceID sourceID, TargetID targetID,
                                  ModMatrix::ConnectionInfo connection) {
        matrix[{sourceID, targetID}] = connection;
    }

    float ModMatrix::getModulatedValue(TargetID targetID) {
        if (targetValues.contains(targetID)) return targetValues[targetID];
        else return 0;
    }

    void ModMatrix::calculateTargetValues() {
        std::unordered_map<TargetID, float> calculatedTargetValues;
        for (const auto &kv: matrix) {
            const auto &sourceID = kv.first.first;
            const auto &targetID = kv.first.second;
            const auto &connection = kv.second;

            calculatedTargetValues[targetID] += sourceValues[sourceID] * connection.depth;
        }

        targetValues = calculatedTargetValues;
    }

    void ModMatrix::setSourceValue(SourceID sourceID, float value) {
        sourceValues[sourceID] = value;
    }
}