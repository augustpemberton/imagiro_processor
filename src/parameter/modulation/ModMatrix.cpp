//
// Created by August Pemberton on 27/01/2025.
//

#include "ModMatrix.h"

namespace imagiro {
    void ModMatrix::removeConnection(const SourceID& sourceID, TargetID targetID) {
        matrix.erase({sourceID, targetID});
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

    std::set<int> ModMatrix::getAlteredTargetVoices(const TargetID& id) {
        if (!targetValues.contains(id)) return {};
        return targetValues[id].alteredVoiceValues;
    }

    void ModMatrix::calculateTargetValues(int numSamples) {
        for (auto& [targetID, targetValue] : targetValues) {
            targetValues[targetID].globalModValue = 0;
            auto& v = targetValues[targetID].voiceModValues;
            std::fill(v.begin(), v.end(), 0.f);
            targetValues[targetID].alteredVoiceValues.clear();
        }

        for (auto& [targetID, numSources] : numModSources) {
            numSources = 0;
        }

        // TODO clamp 0-1 ?
        std::unordered_map<SourceID, std::unordered_set<size_t>> activeVoicesForSource;
        std::unordered_map<TargetID, std::unordered_set<size_t>> activeVoicesForTarget;
        for (auto &[ids, connection] : matrix) {
            const auto &[sourceID, targetID] = ids;

            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID))
                continue;

            numModSources[targetID]++;

            auto connectionSettings = connection.getSettings();

            auto globalValueAddition = sourceValues[sourceID].globalModValue;
            connection.getGlobalEnvelopeFollower().setTargetValue(globalValueAddition);
            connection.getGlobalEnvelopeFollower().skip(numSamples);
            auto v = connection.getGlobalEnvelopeFollower().getCurrentValue() * connectionSettings.depth;
            if (connectionSettings.bipolar) v *= 0.5f;
            targetValues[targetID].globalModValue += v;

            for (size_t i : sourceValues[sourceID].alteredVoiceValues) {
                auto voiceValueAddition = sourceValues[sourceID].voiceModValues[i];
                connection.getVoiceEnvelopeFollower(i).setTargetValue(voiceValueAddition);
                connection.getVoiceEnvelopeFollower(i).skip(numSamples);
                auto va = connection.getVoiceEnvelopeFollower(i).getCurrentValue() * connectionSettings.depth;
                if (connectionSettings.bipolar) va *= 0.5f;
                targetValues[targetID].voiceModValues[i] += va;
                targetValues[targetID].alteredVoiceValues.insert((int)i);

                if (!almostEqual(voiceValueAddition, 0.f) || !almostEqual(connection.getVoiceEnvelopeFollower(i).getCurrentValue(), 0.f)) {
                    activeVoicesForSource[sourceID].insert(i);
                    activeVoicesForTarget[targetID].insert(i);
                }
            }
        }

        for (auto& [sourceID, sourceValue] : sourceValues) {
            if (auto it = activeVoicesForSource.find(sourceID); it != activeVoicesForSource.end()) {
                auto& alteredValues = sourceValue.alteredVoiceValues;
                std::erase_if(alteredValues, [&activeSet = it->second](auto v) {
                    return !activeSet.contains(static_cast<size_t>(v));
                });
            }
        }

        for (auto& [targetID, targetValue] : targetValues) {
            if (auto it = activeVoicesForTarget.find(targetID); it != activeVoicesForTarget.end()) {
                auto& alteredValues = targetValue.alteredVoiceValues;
                std::erase_if(alteredValues, [&activeSet = it->second](auto v) {
                    return !activeSet.contains(static_cast<size_t>(v));
                });
            }
        }
    }

    void ModMatrix::setGlobalSourceValue(const SourceID& sourceID, float value) {
        sourceValues[sourceID].globalModValue = value;
    }

    void ModMatrix::setVoiceSourceValue(const SourceID& sourceID, size_t voiceIndex, float value) {
        sourceValues[sourceID].voiceModValues[voiceIndex] = value;

        if (!almostEqual(value, 0.f)) {
            sourceValues[sourceID].alteredVoiceValues.insert((int)voiceIndex);
        }
    }


    void ModMatrix::registerSource(const SourceID& id, std::string name, SourceType type, bool isBipolar) {
        jassert(!sourceValues.contains(id));

        if (name.empty()) {
            name = id;
        }

        SourceValue sourceValue;
        sourceValue.bipolar = isBipolar;
        sourceValue.type = type;
        sourceValue.name = name;
        sourceValues.insert({id, sourceValue});
    }

    void ModMatrix::registerTarget(const TargetID& id, std::string name) {
        jassert(!targetValues.contains(id));

        if (name.empty()) {
            name = id;
        }

        TargetValue targetValue;
        targetValue.name = name;
        targetValues.insert({id, targetValue});
    }

    int ModMatrix::getNumModSources(const imagiro::TargetID& targetID) {
        return numModSources[targetID];
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
                                  Connection(sampleRate, {entry.depth, entry.attackMS, entry.releaseMS})
                          });
        }
    }
}