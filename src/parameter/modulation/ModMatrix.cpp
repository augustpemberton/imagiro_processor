//
// Created by August Pemberton on 27/01/2025.
//

#include "ModMatrix.h"

namespace imagiro {
    void ModMatrix::removeConnection(const SourceID& sourceID, TargetID targetID) {
        matrix.erase({sourceID, targetID});
        listeners.call(&Listener::OnMatrixUpdated);
    }

    void ModMatrix::removeConnectionsWithSource(const SourceID& sourceID) {
        erase_if(matrix, [sourceID](const auto& entry) {
            return sourceID == entry.first.first;
        });

        listeners.call(&Listener::OnMatrixUpdated);
    }

    void ModMatrix::removeConnectionsWithTarget(const TargetID& targetID) {
        erase_if(matrix, [targetID](const auto& entry) {
            return targetID == entry.first.first;
        });

        listeners.call(&Listener::OnMatrixUpdated);
    }

    void ModMatrix::setConnection(const SourceID& sourceID, const TargetID& targetID,
                                  ModMatrix::Connection::Settings settings) {
        if (!sourceValues.contains(sourceID)) {
            jassertfalse;
            return;
        }

        if (!targetValues.contains(targetID)) {
            jassertfalse;
            return;
        }

        if (matrix.contains({sourceID, targetID})) {
            matrix.at({sourceID, targetID}).setSettings(settings);
        } else {
            matrix.insert({{sourceID, targetID}, {sampleRate, settings}});
        }
        listeners.call(&Listener::OnMatrixUpdated);
    }

    float ModMatrix::getModulatedValue(const TargetID& targetID, int voiceIndex) {
        if (voiceIndex < 0) {
            voiceIndex = static_cast<int>(mostRecentVoiceIndex);
        }

        if (targetValues.contains(targetID)) {
            return targetValues[targetID].value.getGlobalValue() + targetValues[targetID].value.getVoiceValue((size_t)voiceIndex);
        }

        return 0;
    }

    void ModMatrix::prepareToPlay(double sr, int /*maxSamplesPerBlock*/) {
        sampleRate = sr;

        for (auto& [pair, connection] : matrix) {
            connection.setSampleRate(sampleRate);
        }
    }

    std::set<size_t> ModMatrix::getAlteredTargetVoices(const TargetID& id) {
        if (!targetValues.contains(id)) return {};
        return targetValues[id].value.getAlteredVoices();
    }

    void ModMatrix::calculateTargetValues(int numSamples) {

        // reset all target values
        for (auto& [targetID, targetValue] : targetValues) {
            targetValue.value.setGlobalValue(0);
            auto alteredVoices = targetValue.value.getAlteredVoices();
            for (const auto& v: alteredVoices) {
                targetValue.value.setVoiceValue(v, 0);
            }
        }

        std::unordered_set<TargetID> updatedTargets;
        for (auto &[ids, connection] : matrix) {
            const auto &[sourceID, targetID] = ids;
            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID)) continue;

            if (!updatedSourcesSinceLastCalculate.contains(sourceID)) continue;
            updatedTargets.insert(targetID);

            auto connectionSettings = connection.getSettings();

            // Update target's global value
            auto globalValueAddition = sourceValues[sourceID].value.getGlobalValue();
            connection.getGlobalEnvelopeFollower().setTargetValue(globalValueAddition);
            connection.getGlobalEnvelopeFollower().skip(numSamples);
            auto v = connection.getGlobalEnvelopeFollower().getCurrentValue() * connectionSettings.depth;
            if (connectionSettings.bipolar) v *= 0.5f; // half depth & half value
            targetValues[targetID].value.setGlobalValue(targetValues[targetID].value.getGlobalValue() + v);

            // Update target's voice values
            for (size_t i : sourceValues[sourceID].value.getAlteredVoices()) {
                auto voiceValueAddition = sourceValues[sourceID].value.getVoiceValue(i);
                connection.getVoiceEnvelopeFollower(i).setTargetValue(voiceValueAddition);
                connection.getVoiceEnvelopeFollower(i).skip(numSamples);
                auto va = connection.getVoiceEnvelopeFollower(i).getCurrentValue() * connectionSettings.depth;
                if (connectionSettings.bipolar) va *= 0.5f;
                targetValues[targetID].value.setVoiceValue(i, targetValues[targetID].value.getVoiceValue(i) + va);
            }
        }

        for (auto& id : updatedTargets) {
            listeners.call(&Listener::OnTargetValueUpdated, id);
        }

        updatedSourcesSinceLastCalculate.clear();
    }

    void ModMatrix::setGlobalSourceValue(const SourceID& sourceID, float value) {
        const auto oldVal = sourceValues[sourceID].value.getGlobalValue();
        sourceValues[sourceID].value.setGlobalValue(value);
        if (oldVal != value) updatedSourcesSinceLastCalculate.insert(sourceID);
    }

    void ModMatrix::setVoiceSourceValue(const SourceID& sourceID, size_t voiceIndex, float value) {
        const auto oldVal = sourceValues[sourceID].value.getVoiceValue(voiceIndex);
        sourceValues[sourceID].value.setVoiceValue(voiceIndex, value);
        if (oldVal != value) updatedSourcesSinceLastCalculate.insert(sourceID);
    }

    void ModMatrix::resetSourceValue(const SourceID& sourceID) {
        sourceValues[sourceID].value.resetValue();
        updatedSourcesSinceLastCalculate.insert(sourceID);
    }

    void ModMatrix::registerSource(const SourceID& id, std::string name, SourceType type, bool isBipolar) {
        if (sourceValues.contains(id)) {
            sourceValues.erase(id);
        }

        if (name.empty()) {
            name = id;
        }

        SourceValue sourceValue;
        sourceValue.bipolar = isBipolar;
        sourceValue.type = type;
        sourceValue.name = name;
        sourceValues.insert({id, sourceValue});
        listeners.call(&Listener::OnMatrixUpdated);
    }

    void ModMatrix::registerTarget(const TargetID& id, std::string name) {
        if (targetValues.contains(id)) {
            targetValues.erase(id);
        }

        if (name.empty()) {
            name = id;
        }

        TargetValue targetValue;
        targetValue.name = name;
        targetValues.insert({id, targetValue});
        listeners.call(&Listener::OnMatrixUpdated);
    }

    SerializedMatrix ModMatrix::getSerializedMatrix() {
        SerializedMatrix serializedMatrix;
        for (const auto& [pair, connection] : matrix) {
            serializedMatrix.push_back({
                   pair.first,
                   pair.second,
                   connection.getSettings().depth,
                   connection.getSettings().attackMS,
                   connection.getSettings().releaseMS,
                   connection.getSettings().bipolar
           });
        };
        return serializedMatrix;
    }

    void ModMatrix::loadSerializedMatrix(const SerializedMatrix &m) {
        matrix.clear();
        for (const auto& entry : m) {
            matrix.insert({
                                  {SourceID(entry.sourceID), TargetID(entry.targetID)},
                                  Connection(sampleRate, {entry.depth, entry.attackMS, entry.releaseMS,
                                                          entry.bipolar})
                          });
        }
    }
}