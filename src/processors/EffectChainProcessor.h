#pragma once
#include "ChainManager.h"
#include "erosion/ErosionProcessor.h"
#include "iir-filter/IIRFilterProcessor.h"
#include "noise/NoiseProcessor.h"
#include "saturation/SaturationProcessor.h"
#include "utility/UtilityProcessor.h"
#include "wobble/WobbleProcessor.h"

enum class EffectType {
    Gain = 0,
    DiffuseDelay = 1,
    Filter = 2,
    Chorus = 3,
    Saturation = 4,
    Wobble = 5,
    Noise = 6,
    Erosion = 7,
};

class EffectChainProcessor : public ChainManager<EffectType, Processor> {
public:
    EffectChainProcessor()
        : ChainManager(true, 2) {
    }

protected:
    std::shared_ptr<Processor> createProcessorForType(const EffectType type, int id) const override {
        if (type == EffectType::Gain) return std::make_shared<UtilityProcessor>();
        if (type == EffectType::DiffuseDelay) return std::make_shared<DiffuseDelayProcessor>();
        if (type == EffectType::Filter) return std::make_shared<IIRFilterProcessor>();
        if (type == EffectType::Chorus) return std::make_shared<ChorusProcessor>();
        if (type == EffectType::Saturation) return std::make_shared<SaturationProcessor>();
        if (type == EffectType::Wobble) return std::make_shared<WobbleProcessor>();
        if (type == EffectType::Noise) return std::make_shared<NoiseProcessor>();
        if (type == EffectType::Erosion) return std::make_shared<ErosionProcessor>();
        return nullptr;
    }

    std::string getModTargetPrefix() const override { return "param-fx-"; }
    std::string getStateObjectName() const override { return "EffectState"; }
    std::string getTypeFieldName() const override { return "EffectType"; }
    std::string getProcessorStateFieldName() const override { return "ProcessorState"; }
    bool shouldPassInputToOutput() const override { return true; }
};
