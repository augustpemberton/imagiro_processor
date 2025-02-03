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

    float ModMatrix::getModulatedValue(TargetID targetID, int voiceIndex) {
        if (voiceIndex < 0) {
            // TODO use most recently played voice instead of voice 0
            voiceIndex = 0;
        }

        if (targetValues.contains(targetID)) {
            return targetValues[targetID].globalModValue + targetValues[targetID].voiceModValues[(size_t)voiceIndex];
        }
        else {
            return 0;
        }
    }

    void ModMatrix::calculateTargetValues() {
        for (auto& [targetID, targetValue] : targetValues) {
            targetValues[targetID].globalModValue = 0;
            auto& v = targetValues[targetID].voiceModValues;
            std::fill(v.begin(), v.end(), 0.f);
        }

        for (auto& [targetID, numSources] : numModSources) {
            numSources = 0;
        }

        // TODO clamp 0-1 ?
        for (const auto &[ids, connection] : matrix) {
            const auto &[sourceID, targetID] = ids;

            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID))
                continue;

            numModSources[targetID]++;

            targetValues[targetID].globalModValue += sourceValues[sourceID].globalModValue * connection.depth;
            for (auto i=0u; i<MAX_VOICES; i++) {
                targetValues[targetID].voiceModValues[i] += sourceValues[sourceID].voiceModValues[i] * connection.depth;
            }
        }
    }

    void ModMatrix::setGlobalSourceValue(SourceID sourceID, float value) {
        sourceValues[sourceID].globalModValue = value;
    }

    void ModMatrix::setVoiceSourceValue(SourceID sourceID, size_t voiceIndex, float value) {
        sourceValues[sourceID].voiceModValues[voiceIndex] = value;
    }


    SourceID ModMatrix::registerSource() {
        auto id = SourceID{nextSourceID++};
        sourceValues.insert({id, {}});
        return id;
    }

    TargetID ModMatrix::registerTarget() {
        auto id = TargetID{nextTargetID++};
        targetValues.insert({id, {}});
        return id;
    }

    int ModMatrix::getNumModSources(imagiro::TargetID targetID) {
        return numModSources[targetID];
    }
}