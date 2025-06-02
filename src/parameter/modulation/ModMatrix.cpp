//
// Created by August Pemberton on 27/01/2025.
//

#include "ModMatrix.h"

namespace imagiro {
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

        if (targetValues.contains(targetID)) {
            return targetValues[targetID]->value.getGlobalValue() + targetValues[targetID]->value.getVoiceValue((size_t)voiceIndex);
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
        return targetValues[id]->value.getAlteredVoices();
    }

    void ModMatrix::calculateTargetValues(int numSamples) {

        std::unordered_set<TargetID> updatedTargets;
        for (auto &[ids, connection] : matrix) {
            const auto &[sourceID, targetID] = ids;
            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID)) continue;

            bool sourceValueUpdated = updatedSourcesSinceLastCalculate.contains(sourceID);
            if (connection.getGlobalEnvelopeFollower().isSmoothing()) sourceValueUpdated = true;
            for (size_t i : sourceValues[sourceID]->value.getAlteredVoices()) {
                if (connection.getVoiceEnvelopeFollower(i).isSmoothing()) sourceValueUpdated = true;
            }

            if (!sourceValueUpdated) continue;

            // if we haven't processed this target yet, reset it to 0 first
            if (!updatedTargets.contains(targetID)) {
                targetValues[targetID]->value.resetValue();
            }

            updatedTargets.insert(targetID);


            auto connectionSettings = connection.getSettings();

            // Update target's global value
            auto globalValueAddition = sourceValues[sourceID]->value.getGlobalValue();
            connection.getGlobalEnvelopeFollower().setTargetValue(globalValueAddition);
            connection.getGlobalEnvelopeFollower().skip(numSamples);
            auto v = connection.getGlobalEnvelopeFollower().getCurrentValue() * connectionSettings.depth;
            if (connectionSettings.bipolar) v *= 0.5f; // half depth & half value
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
            sourceValues.erase(deleteSource);

            erase_if(matrix, [deleteSource](const auto& entry) {
                return entry.first.first == deleteSource;
            });
        }

        TargetID deleteTarget;
        while (targetsToDelete.try_dequeue(deleteTarget)) {
            updated = true;
            targetsToDeallocate.enqueue(targetValues.at(deleteTarget));
            targetValues.erase(deleteTarget);

            erase_if(matrix, [deleteTarget](const auto& entry) {
                return entry.first.second == deleteTarget;
            });
        }

        if (updated) matrixUpdated();
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

            cachedSerializedMatrix = SerializedMatrix();
            for (const auto& [pair, connection] : matrix) {
                cachedSerializedMatrix.push_back({
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