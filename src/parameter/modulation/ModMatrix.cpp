//
// Created by August Pemberton on 27/01/2025.
//

#include "ModMatrix.h"

#include <MacTypes.h>

namespace imagiro {
    ModMatrix::ModMatrix() {
        cachedSerializedMatrix.ensureStorageAllocated(MAX_MOD_CONNECTIONS);
        matrix.reserve(MAX_MOD_CONNECTIONS);
    }

    ModMatrix::~ModMatrix() {
        // listeners.call(&Listener::OnMatrixDestroyed, *this);
    }

    void ModMatrix::removeConnection(const SourceID& sourceID, TargetID targetID) {
        matrix.erase({sourceID, targetID});
        updatedSourcesSinceLastCalculate.insert(sourceID);
        updatedTargetsSinceLastCalculate.insert(targetID);

        listeners.call(&Listener::OnConnectionRemoved, sourceID, targetID);
        recacheSerializedMatrixFlag = true;
    }

    void ModMatrix::queueConnection(const SourceID& sourceID, const TargetID& targetID,
                                    Connection::Settings connectionSettings) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        updatedConnectionsQueue.enqueue({sourceID, targetID, connectionSettings});
    }

    void ModMatrix::setConnection(const SourceID& sourceID, const TargetID& targetID,
                                  ModMatrix::Connection::Settings settings) {
        jassert(!juce::MessageManager::getInstance()->isThisTheMessageThread());
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
        updatedTargetsSinceLastCalculate.insert(targetID);
    }

    float ModMatrix::getModulatedValue(const TargetID& targetID, int voiceIndex) {
        if (voiceIndex < 0) {
            voiceIndex = static_cast<int>(mostRecentVoiceIndex);
        }

        if (targetValues.empty()) return 0.f;
        if (const auto& v = targetValues[targetID]) {
            return v->value.getGlobalValue() + v->value.getVoiceValue(static_cast<size_t>(voiceIndex));
        }

        return 0;
    }

    void ModMatrix::prepareToPlay(double sr, int /*maxSamplesPerBlock*/) {
        sampleRate = sr;

        for (auto& [pair, connection]: matrix) {
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

        for (auto& [ids, connection]: matrix) {
            const auto& [sourceID, targetID] = ids;
            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID)) continue;

            bool sourceValueUpdated = updatedSourcesSinceLastCalculate.contains(sourceID);
            if (connection.getGlobalEnvelopeFollower().isSmoothing()) sourceValueUpdated = true;
            for (const size_t i: sourceValues[sourceID]->value.getAlteredVoices()) {
                if (connection.getVoiceEnvelopeFollower(i).isSmoothing()) sourceValueUpdated = true;
            }

            if (sourceValueUpdated) {
                targetsToUpdate.insert(targetID);
            }
        }

        for (const auto& targetID: updatedTargetsSinceLastCalculate) {
            targetsToUpdate.insert(targetID);
        }


        for (const auto& targetID: targetsToUpdate) {
            if (!targetValues.contains(targetID)) continue;
            targetValues[targetID]->value.resetValue();
        }

        // Second pass: calculate values for targets that need updating
        for (auto& [ids, connection]: matrix) {
            const auto& [sourceID, targetID] = ids;
            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID)) continue;
            if (!targetsToUpdate.contains(targetID)) continue;
            if (!targetValues.contains(targetID)) {
                DBG("A");
            }

            updatedTargets.insert(targetID);

            auto connectionSettings = connection.getSettings();

            // Update target's global value
            auto globalValueAddition = sourceValues[sourceID]->value.getGlobalValue();
            connection.getGlobalEnvelopeFollower().setTargetValue(globalValueAddition);
            connection.getGlobalEnvelopeFollower().skip(numSamples);
            auto v = connection.getGlobalEnvelopeFollower().getCurrentValue() * connectionSettings.depth;
            if (sourceValues[sourceID]->bipolar) v *= 0.5f;
            targetValues[targetID]->value.setGlobalValue(targetValues[targetID]->value.getGlobalValue() + v);
            listeners.call(&Listener::OnTargetValueUpdated, targetID, -1);

            // Update target's voice values
            for (size_t i: sourceValues[sourceID]->value.getAlteredVoices()) {
                auto voiceValueAddition = sourceValues[sourceID]->value.getVoiceValue(i);
                connection.getVoiceEnvelopeFollower(i).setTargetValue(voiceValueAddition);
                connection.getVoiceEnvelopeFollower(i).skip(numSamples);
                auto va = connection.getVoiceEnvelopeFollower(i).getCurrentValue() * connectionSettings.depth;
                if (sourceValues[sourceID]->bipolar) va *= 0.5f;
                targetValues[targetID]->value.setVoiceValue(targetValues[targetID]->value.getVoiceValue(i) + va, i);
                listeners.call(&Listener::OnTargetValueUpdated, targetID, i);
            }

            targetValues[targetID]->value.removeResetVoices();
        }

        updatedSourcesSinceLastCalculate.clear();
        updatedTargetsSinceLastCalculate.clear();
    }

    void ModMatrix::setGlobalSourceValue(const SourceID& sourceID, float value) {
        if (!sourceValues.contains(sourceID)) return;
        const auto oldVal = sourceValues[sourceID]->value.getGlobalValue();
        sourceValues[sourceID]->value.setGlobalValue(value);

        if (oldVal != value) {
            updatedSourcesSinceLastCalculate.insert(sourceID);
            listeners.call(&Listener::OnSourceValueUpdated, sourceID, -1);
        }
    }

    void ModMatrix::setVoiceSourceValue(const SourceID& sourceID, size_t voiceIndex, float value) {
        if (!sourceValues.contains(sourceID)) return;
        const auto oldVal = sourceValues[sourceID]->value.getVoiceValue(voiceIndex);
        sourceValues[sourceID]->value.setVoiceValue(value, voiceIndex);
        if (oldVal != value) {
            updatedSourcesSinceLastCalculate.insert(sourceID);
            listeners.call(&Listener::OnSourceValueUpdated, sourceID, voiceIndex);
        }
    }

    void ModMatrix::resetSourceValue(const SourceID& sourceID) {
        if (!sourceValues.contains(sourceID)) return;
        sourceValues[sourceID]->value.resetValue();
        updatedSourcesSinceLastCalculate.insert(sourceID);
    }

    void ModMatrix::processMatrixUpdates() {
        std::pair<SourceID, std::shared_ptr<SourceValue> > newSource;
        while (newSourcesQueue.try_dequeue(newSource)) {
            if (sourceValues.contains(newSource.first)) {
                sourceValues.erase(newSource.first);
            }
            sourceValues.insert(newSource);
            listeners.call(&Listener::OnSourceValueAdded, newSource.first);
        }

        std::pair<TargetID, std::shared_ptr<TargetValue> > newTarget;
        while (newTargetsQueue.try_dequeue(newTarget)) {
            if (targetValues.contains(newTarget.first)) {
                targetValues.erase(newTarget.first);
            }
            targetValues.insert(newTarget);
            listeners.call(&Listener::OnTargetValueAdded, newTarget.first);
        }

        SourceID deleteSource;
        while (sourcesToDelete.try_dequeue(deleteSource)) {
            sourcesToDeallocate.enqueue(sourceValues.at(deleteSource));
            sourceValues[deleteSource]->value.resetValue();
            sourceValues.erase(deleteSource);

            erase_if(matrix, [&, deleteSource](const auto& entry) {
                if (entry.first.first == deleteSource) {
                    // reset any linked target values here, as if we are deleting the last connected source to that target,
                    // it won't get recalculated to 0 as it's not in the matrix anymore
                    if (targetValues.contains(entry.first.second)) {
                        targetValues.at(entry.first.second)->value.resetValue();
                        listeners.call(&Listener::OnTargetValueReset, entry.first.second);
                    }
                    return true;
                }
            });

            listeners.call(&Listener::OnSourceValueRemoved, deleteSource);
        }

        TargetID deleteTarget;
        while (targetsToDelete.try_dequeue(deleteTarget)) {
            targetsToDeallocate.enqueue(targetValues.at(deleteTarget));
            targetValues[deleteTarget]->value.resetValue();

            erase_if(matrix, [deleteTarget](const auto& entry) {
                return entry.first.second == deleteTarget;
            });

            listeners.call(&Listener::OnTargetValueRemoved, deleteSource);
        }

        ConnectionDefinition newConnection;
        while (updatedConnectionsQueue.try_dequeue(newConnection)) {
            bool connectionExisted = matrix.contains({newConnection.sourceID, newConnection.targetID});
            setConnection(newConnection.sourceID, newConnection.targetID, newConnection.connectionSettings);
            recacheSerializedMatrixFlag = true;
            if (connectionExisted) {
                listeners.call(&Listener::OnConnectionUpdated, newConnection.sourceID, newConnection.targetID);
            } else {
                listeners.call(&Listener::OnConnectionAdded, newConnection.sourceID, newConnection.targetID);
            }
        }
    }

    SourceID ModMatrix::registerSource(std::string name, const SourceType type, const bool isBipolar) {
        const auto id = nextSourceID++;

        if (name.empty()) {
            name = "source" + std::to_string(id);
        }

        updateSource(id, name, type, isBipolar);
        return id;
    }

    void ModMatrix::updateSource(SourceID id, const std::string& name, const SourceType type, const bool isBipolar) {
        auto sourceValue = std::make_shared<SourceValue>();
        sourceValue->bipolar = isBipolar;
        sourceValue->type = type;
        sourceValue->name = name;
        newSourcesQueue.enqueue({id, sourceValue});
    }

    TargetID ModMatrix::registerTarget(std::string name) {
        auto id = nextTargetID++;
        if (name.empty()) {
            name = id;
        }

        auto targetValue = std::make_shared<TargetValue>();
        targetValue->name = name;
        newTargetsQueue.enqueue({id, targetValue});
        return id;
    }

    SerializedMatrix ModMatrix::getSerializedMatrix() {
        if (recacheSerializedMatrixFlag) {
            recacheSerializedMatrixFlag = false;

            cachedSerializedMatrix.clearQuick();
            for (const auto& [pair, connection]: matrix) {
                cachedSerializedMatrix.add({
                    pair.first,
                    pair.second,
                    connection.getSettings().depth,
                    connection.getSettings().attackMS,
                    connection.getSettings().releaseMS,
                });
            }
        }

        return cachedSerializedMatrix;
    }

    void ModMatrix::loadSerializedMatrix(const SerializedMatrix& m) {
        matrix.clear();
        for (const auto& entry: m) {
            matrix.insert({
                {SourceID(entry.sourceID), TargetID(entry.targetID)},
                Connection(sampleRate, {entry.depth, entry.attackMS, entry.releaseMS})
            });
        }
    }
}
