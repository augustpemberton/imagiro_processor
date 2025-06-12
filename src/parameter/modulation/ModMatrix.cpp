//
// Created by August Pemberton on 27/01/2025.
//

#include "ModMatrix.h"

#include <MacTypes.h>

namespace imagiro {
    ModMatrix::ModMatrix () {
        cachedSerializedMatrix.ensureStorageAllocated(MAX_MOD_CONNECTIONS);
        matrix.reserve(MAX_MOD_CONNECTIONS);
    }

    void ModMatrix::removeConnection(const SourceID& sourceID, TargetID targetID) {
        matrix.erase({sourceID, targetID});
        matrixUpdated();
    }

    void ModMatrix::matrixUpdated() {
        recacheSerializedMatrixFlag = true;
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

        // TODO make sure this happens on the audio thread
        if (matrix.contains({sourceID, targetID})) {
            matrix.at({sourceID, targetID}).setSettings(settings);
        } else {
            matrix.insert({{sourceID, targetID}, {sampleRate, settings}});
        }

        updatedSourcesSinceLastCalculate.insert(sourceID);
        matrixUpdated();
    }

    float ModMatrix::getModulatedValue(const TargetID& targetID, int voiceIndex) {
        if (voiceIndex < 0) {
            voiceIndex = static_cast<int>(mostRecentVoiceIndex);
        }

        jassert(targetValues.contains(targetID));
        if (const auto& v = targetValues[targetID]) {
            return v->value.getGlobalValue() + v->value.getVoiceValue(static_cast<size_t>(voiceIndex));
        }

        return 0;
    }

    void ModMatrix::prepareToPlay(double sr, int /*maxSamplesPerBlock*/) {
        sampleRate = sr;

        for (auto& [pair, connection] : matrix) {
            connection.setSampleRate(sampleRate);
        }
    }

    FixedHashSet<size_t, MAX_MOD_VOICES> ModMatrix::getAlteredTargetVoices(const TargetID& id) {
        if (!targetValues.contains(id)) return {};
        return targetValues[id]->value.getAlteredVoices();
    }

    void ModMatrix::calculateTargetValues(int numSamples) {
        updatedTargets.clear();
        FixedHashSet<TargetID, MAX_MOD_TARGETS> targetsToUpdate;

        for (auto &[ids, connection] : matrix) {
            const auto &[sourceID, targetID] = ids;
            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID)) continue;

            bool sourceValueUpdated = updatedSourcesSinceLastCalculate.contains(sourceID);
            if (connection.getGlobalEnvelopeFollower().isSmoothing()) sourceValueUpdated = true;
            for (const size_t i : sourceValues[sourceID]->value.getAlteredVoices()) {
                if (connection.getVoiceEnvelopeFollower(i).isSmoothing()) sourceValueUpdated = true;
            }

            if (sourceValueUpdated) {
                targetsToUpdate.insert(targetID);
            }
        }

        for (const auto& targetID : targetsToUpdate) {
            targetValues[targetID]->value.resetValue();
        }

        // Second pass: calculate values for targets that need updating
        for (auto &[ids, connection] : matrix) {
            const auto &[sourceID, targetID] = ids;
            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID)) continue;
            if (!targetsToUpdate.contains(targetID)) continue;

            updatedTargets.insert(targetID);

            auto connectionSettings = connection.getSettings();

            // Update target's global value
            auto globalValueAddition = sourceValues[sourceID]->value.getGlobalValue();
            connection.getGlobalEnvelopeFollower().setTargetValue(globalValueAddition);
            connection.getGlobalEnvelopeFollower().skip(numSamples);
            auto v = connection.getGlobalEnvelopeFollower().getCurrentValue() * connectionSettings.depth;
            if (connectionSettings.bipolar) v *= 0.5f;
            targetValues[targetID]->value.setGlobalValue(targetValues[targetID]->value.getGlobalValue() + v);

            // Update target's voice values
            for (size_t i : sourceValues[sourceID]->value.getAlteredVoices()) {
                auto voiceValueAddition = sourceValues[sourceID]->value.getVoiceValue(i);
                connection.getVoiceEnvelopeFollower(i).setTargetValue(voiceValueAddition);
                connection.getVoiceEnvelopeFollower(i).skip(numSamples);
                auto va = connection.getVoiceEnvelopeFollower(i).getCurrentValue() * connectionSettings.depth;
                if (connectionSettings.bipolar) va *= 0.5f;
                targetValues[targetID]->value.setVoiceValue(targetValues[targetID]->value.getVoiceValue(i) + va, i);
            }
        }

        for (auto& id : updatedTargets) {
            listeners.call(&Listener::OnTargetValueUpdated, id);
        }

        updatedSourcesSinceLastCalculate.clear();
    }

    void ModMatrix::setGlobalSourceValue(const SourceID& sourceID, float value) {
        if (!sourceValues.contains(sourceID)) return;
        const auto oldVal = sourceValues[sourceID]->value.getGlobalValue();
        sourceValues[sourceID]->value.setGlobalValue(value);
        if (oldVal != value) updatedSourcesSinceLastCalculate.insert(sourceID);
    }

    void ModMatrix::setVoiceSourceValue(const SourceID& sourceID, size_t voiceIndex, float value) {
        if (!sourceValues.contains(sourceID)) return;
        const auto oldVal = sourceValues[sourceID]->value.getVoiceValue(voiceIndex);
        sourceValues[sourceID]->value.setVoiceValue(value, voiceIndex);
        if (oldVal != value) updatedSourcesSinceLastCalculate.insert(sourceID);
    }

    void ModMatrix::resetSourceValue(const SourceID& sourceID) {
        sourceValues[sourceID]->value.resetValue();
        updatedSourcesSinceLastCalculate.insert(sourceID);
    }

    void ModMatrix::processMatrixUpdates() {
        bool updated = false;
        std::unordered_set<TargetID> updatedTargets;

        std::pair<SourceID, std::shared_ptr<SourceValue>> newSource;
        while (newSourcesQueue.try_dequeue(newSource)) {
            updated = true;
            if (sourceValues.contains(newSource.first)) {
                sourceValues.erase(newSource.first);
            }
            sourceValues.insert(newSource);
        }

        std::pair<TargetID, std::shared_ptr<TargetValue>> newTarget;
        while (newTargetsQueue.try_dequeue(newTarget)) {
            updated = true;
            if (targetValues.contains(newTarget.first)) {
                targetValues.erase(newTarget.first);
            }
            targetValues.insert(newTarget);
        }

        SourceID deleteSource;
        while (sourcesToDelete.try_dequeue(deleteSource)) {
            updated = true;
            sourcesToDeallocate.enqueue(sourceValues.at(deleteSource));
            sourceValues[deleteSource]->value.resetValue();

            erase_if(matrix, [&, deleteSource](const auto& entry) {
                if (entry.first.first == deleteSource) {
                    // reset any linked target values here, as if we are deleting the last connected source to that target,
                    // it won't get recalculated to 0 as it's not in the matrix anymore
                    if (targetValues.contains(entry.first.second)) {
                        targetValues.at(entry.first.second)->value.resetValue();
                        updatedTargets.insert(entry.first.second);
                    }
                    return true;
                }
            });
        }

        TargetID deleteTarget;
        while (targetsToDelete.try_dequeue(deleteTarget)) {
            updated = true;
            targetsToDeallocate.enqueue(targetValues.at(deleteTarget));
            targetValues[deleteTarget]->value.resetValue();
            updatedTargets.insert(deleteTarget);

            erase_if(matrix, [deleteTarget](const auto& entry) {
                return entry.first.second == deleteTarget;
            });
        }

        // update matrix before calling target value updated listeners
        // as we use the matrix to get the value after the listener is called,
        // so the matrix needs to be up to date first
        if (updated) matrixUpdated();
        for (const auto& target : updatedTargets) {
            listeners.call(&Listener::OnTargetValueUpdated, target);
        }
    }

    void ModMatrix::registerSource(const SourceID& id, std::string name, SourceType type, bool isBipolar) {
        if (name.empty()) {
            name = id;
        }

        auto sourceValue = std::make_shared<SourceValue>();
        sourceValue->bipolar = isBipolar;
        sourceValue->type = type;
        sourceValue->name = name;
        newSourcesQueue.enqueue({id, sourceValue});
    }

    void ModMatrix::registerTarget(const TargetID& id, std::string name) {

        if (name.empty()) {
            name = id;
        }

        auto targetValue = std::make_shared<TargetValue>();
        targetValue->name = name;
        newTargetsQueue.enqueue({id, targetValue});
    }

    SerializedMatrix ModMatrix::getSerializedMatrix() {
        if (recacheSerializedMatrixFlag) {
            recacheSerializedMatrixFlag = false;

            cachedSerializedMatrix.clearQuick();
            for (const auto& [pair, connection] : matrix) {
                cachedSerializedMatrix.add({
                       pair.first,
                       pair.second,
                       connection.getSettings().depth,
                       connection.getSettings().attackMS,
                       connection.getSettings().releaseMS,
                       connection.getSettings().bipolar
               });
            }
        }

        return cachedSerializedMatrix;
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
