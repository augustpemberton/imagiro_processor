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
        if (targetValues.contains(targetID)) return std::get<float>(targetValues[targetID].value);
        else return 0;
    }

    float ModMatrix::getModulatedValue(TargetID targetID, size_t voiceIndex) {
        if (targetValues.contains(targetID)) {
            auto& valueArray = std::get<std::array<float, MAX_VOICES>>(targetValues[targetID].value);
            return valueArray[voiceIndex];
        }
        else return 0;
    }

    void ModMatrix::calculateTargetValues() {
        for (auto& [targetID, targetValue] : targetValues) {
            if (std::holds_alternative<float>(targetValue.value)) {
                std::get<float>(targetValue.value) = 0.0f;
            } else {
                auto& arr = std::get<std::array<float, MAX_VOICES>>(targetValue.value);
                std::fill(arr.begin(), arr.end(), 0.0f);
            }
        }

        for (auto& [targetID, numSources] : numModSources) {
            numSources = 0;
        }

        // TODO clamp 0-1 ?
        for (const auto &[ids, connection] : matrix) {
            const auto &[sourceID, targetID] = ids;

            if (!sourceValues.contains(sourceID) || !targetValues.contains(targetID))
                continue;

            auto sourceType = sourceValues[sourceID].type;
            auto targetType = targetValues[targetID].type;

            numModSources[targetID]++;

            if (sourceType == ModulationType::Global && targetType == ModulationType::Global) {
                auto& sourceValue = std::get<float>(sourceValues[sourceID].value);
                auto& targetValue = std::get<float>(targetValues[targetID].value);
                targetValue += sourceValue * connection.depth;
            }
            else if (sourceType == ModulationType::PerVoice && targetType == ModulationType::Global) {
                auto& sourceValue = std::get<std::array<float, MAX_VOICES>>(sourceValues[sourceID].value);
                auto& targetValue = std::get<float>(targetValues[targetID].value);
                targetValue += sourceValue[0] * connection.depth; // TODO: use most recently played voice
            }
            else if (sourceType == ModulationType::Global && targetType == ModulationType::PerVoice) {
                auto& sourceValue = std::get<float>(sourceValues[sourceID].value);
                auto& targetValue = std::get<std::array<float, MAX_VOICES>>(targetValues[targetID].value);
                for (auto& target : targetValue) {
                    target += sourceValue * connection.depth;
                }
            } else if (sourceType == ModulationType::PerVoice && targetType == ModulationType::PerVoice) {
                auto& sourceValue = std::get<std::array<float, MAX_VOICES>>(sourceValues[sourceID].value);
                auto& targetValue = std::get<std::array<float, MAX_VOICES>>(targetValues[targetID].value);
                for (auto i=0u; i<MAX_VOICES; i++) {
                    targetValue[i] += sourceValue[i] * connection.depth;
                }
            }
        }
    }

    void ModMatrix::setSourceValue(SourceID sourceID, float value) {
        jassert(std::holds_alternative<float>(sourceValues[sourceID].value));
        sourceValues[sourceID].value = value;
    }

    void ModMatrix::setSourceValue(SourceID sourceID, size_t voiceIndex, float value) {
        jassert((std::holds_alternative<std::array<float, MAX_VOICES>>(sourceValues[sourceID].value)));

        auto& valueArray = std::get<std::array<float, MAX_VOICES>>(sourceValues[sourceID].value);
        valueArray[voiceIndex] = value;
    }


    SourceID ModMatrix::registerSource(ModulationType type) {
        auto id = SourceID{nextSourceID++};
        auto value = getDefaultVariantValue(type);
        SourceValue tv {type, value};
        sourceValues.insert({id, tv});
        return id;
    }

    TargetID ModMatrix::registerTarget(ModulationType type) {
        auto id = TargetID{nextTargetID++};
        auto value = getDefaultVariantValue(type);
        TargetValue tv {type, value};
        targetValues.insert({id, tv});
        return id;
    }

    int ModMatrix::getNumModSources(imagiro::TargetID targetID) {
        return numModSources[targetID];
    }
}