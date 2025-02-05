//
// Created by August Pemberton on 27/01/2025.
//

#include "ModMatrix.h"

namespace imagiro {
    void ModMatrix::removeConnection(SourceID sourceID, TargetID targetID) {
        matrix.erase({sourceID, targetID});
        listeners.call(&Listener::OnMatrixUpdated);
    }

    void ModMatrix::setConnection(SourceID sourceID, TargetID targetID,
                                  ModMatrix::Connection::Settings settings) {
        if (matrix.contains({sourceID, targetID})) {
            matrix.at({sourceID, targetID}).setSettings(settings);
        } else {
            matrix.insert({{sourceID, targetID}, {sampleRate, settings}});
        }
        listeners.call(&Listener::OnMatrixUpdated);
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

    void ModMatrix::prepareToPlay(double sr, int /*maxSamplesPerBlock*/) {
        sampleRate = sr;

        for (auto& [pair, connection] : matrix) {
            connection.setSampleRate(sampleRate);
        }
    }

    void ModMatrix::calculateTargetValues(int numSamples) {
        for (auto& [targetID, targetValue] : targetValues) {
            targetValues[targetID].globalModValue = 0;
            auto& v = targetValues[targetID].voiceModValues;
            std::fill(v.begin(), v.end(), 0.f);
        }

        for (auto& [targetID, numSources] : numModSources) {
            numSources = 0;
        }

        // TODO clamp 0-1 ?
        for (auto &[ids, connection] : matrix) {
            const auto &[sourceID, targetID] = ids;

            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID))
                continue;

            numModSources[targetID]++;

            auto connectionSettings = connection.getSettings();

            auto globalValueAddition = sourceValues[sourceID].globalModValue;
            connection.getGlobalEnvelopeFollower().setTargetValue(globalValueAddition);
            connection.getGlobalEnvelopeFollower().skip(numSamples);
            targetValues[targetID].globalModValue += connection.getGlobalEnvelopeFollower().getCurrentValue() * connectionSettings.depth;

            for (auto i : sourceValues[sourceID].alteredVoiceValues) {
                auto voiceValueAddition = sourceValues[sourceID].voiceModValues[i];
                connection.getVoiceEnvelopeFollower(i).setTargetValue(voiceValueAddition);
                connection.getVoiceEnvelopeFollower(i).skip(numSamples);
                targetValues[targetID].voiceModValues[i] += connection.getVoiceEnvelopeFollower(i).getCurrentValue() * connectionSettings.depth;
            }
        }
    }

    void ModMatrix::setGlobalSourceValue(SourceID sourceID, float value) {
        sourceValues[sourceID].globalModValue = value;
    }

    void ModMatrix::setVoiceSourceValue(SourceID sourceID, size_t voiceIndex, float value) {
        sourceValues[sourceID].voiceModValues[voiceIndex] = value;

        if (!almostEqual(value, 0.f)) {
            sourceValues[sourceID].alteredVoiceValues.insert((int)voiceIndex);
        } else {
            sourceValues[sourceID].alteredVoiceValues.erase((int)voiceIndex);
        }
    }


    SourceID ModMatrix::registerSource(std::string name) {
        auto id = SourceID{nextSourceID++};
        sourceValues.insert({id, {}});

        if (name.empty()) {
             name = "source " + std::to_string(id);
        }

        sourceNames.insert({id, name});

        return id;
    }

    TargetID ModMatrix::registerTarget(std::string name) {
        auto id = TargetID{nextTargetID++};
        targetValues.insert({id, {}});

        if (name.empty()) {
            name = "target " + std::to_string(id);
        }

        targetNames.insert({id, name});

        return id;
    }

    int ModMatrix::getNumModSources(imagiro::TargetID targetID) {
        return numModSources[targetID];
    }

    SerializedMatrix ModMatrix::getSerializedMatrix() {
        SerializedMatrix serializedMatrix;
        for (const auto& [pair, connection] : matrix) {
            serializedMatrix.push_back({
                   static_cast<int>(pair.first),
                   static_cast<int>(pair.second),
                   connection.getSettings().depth,
                   connection.getSettings().attackMS,
                   connection.getSettings().releaseMS
           });
        };
        return serializedMatrix;
    }

    void ModMatrix::loadSerializedMatrix(const SerializedMatrix &m) {
        matrix.clear();
        for (const auto& entry : m) {
            matrix.insert({
                                  {SourceID(entry.sourceID), TargetID(entry.targetID)},
                                  Connection(sampleRate, {entry.depth, entry.attackMS, entry.releaseMS})
                          });
        }
    }
}