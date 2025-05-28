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

        // std::unordered_map<SourceID, std::unordered_set<size_t>> activeVoicesForSource;
        // std::unordered_map<TargetID, std::unordered_set<size_t>> activeVoicesForTarget;
        for (auto &[ids, connection] : matrix) {
            const auto &[sourceID, targetID] = ids;

            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID)) continue;

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

                // Store voices if active so we can update active voices array later
                // if (!almostEqual(voiceValueAddition, 0.f) || !almostEqual(connection.getVoiceEnvelopeFollower(i).getCurrentValue(), 0.f)) {
                //     activeVoicesForSource[sourceID].insert(i);
                //     activeVoicesForTarget[targetID].insert(i);
                // }
            }
        }

        // reset inactive sourceValue voice channels
        // for (auto& [sourceID, sourceValue] : sourceValues) {
        //     if (auto it = activeVoicesForSource.find(sourceID); it != activeVoicesForSource.end()) {
        //         auto alteredValues = sourceValue.value.getAlteredVoices();
        //         for (auto v : alteredValues) {
        //             if (!it->second.contains(v)) {
        //                 sourceValue.value.setVoiceValue(v, 0);
        //             }
        //         }
        //     }
        // }
    }

    void ModMatrix::setGlobalSourceValue(const SourceID& sourceID, float value) {
        sourceValues[sourceID].value.setGlobalValue(value);
    }

    void ModMatrix::setVoiceSourceValue(const SourceID& sourceID, size_t voiceIndex, float value) {
        sourceValues[sourceID].value.setVoiceValue(voiceIndex, value);
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