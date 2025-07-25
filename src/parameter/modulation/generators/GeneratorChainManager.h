#pragma once
#include "ModGenerator.h"
#include "CurveLFO/CurveLFOGenerator.h"
#include "Envelope/EnvelopeGenerator.h"
#include "EnvelopeFollower/EnvelopeFollowerGenerator.h"
#include "EnvelopeFollower/EnvelopeFollowerSources.h"
#include "imagiro_processor/src/synth/MPESynth.h"
#include "Macro/MacroGenerator.h"
#include "MultichannelValue/MultichannelValueGenerator.h"
#include "MultichannelValue/MultichannelValueSources.h"

enum class GeneratorType {
    LFO = 0,
    Macro = 1,
    EnvelopeFollower = 2,
    MIDI = 3,
    Envelope = 4,
};

class GeneratorChainManager : public ChainManager<GeneratorType, ModGenerator> {
public:
    GeneratorChainManager(const ParameterFactory& parameterFactory, ModMatrix &m, const int numSlots, const int maxParamsPerSlot)
    : ChainManager(parameterFactory, numSlots, maxParamsPerSlot, 0), modMatrix(m) {
    }

    void setEnvelopeSynth(MPESynth& synth) {
        this->synth = &synth;
    }

    void setMIDISources(MultichannelValueSources& sources) {
        midiSources = &sources;
    }

    void setEnvelopeFollowerSources(EnvelopeFollowerSources& sources) {
        envSources = &sources;
    }

    static std::string to_string(const GeneratorType type) {
        switch (type) {
            case GeneratorType::LFO: return "LFO";
            case GeneratorType::Macro: return "Macro";
            case GeneratorType::EnvelopeFollower: return "Env Follow";
            case GeneratorType::Envelope: return "Envelope";
            case GeneratorType::MIDI: return "MIDI";
        }
        return "";
    }

    choc::value::Value getItemState (const Item& item) override {
        auto state = ChainManager::getItemState(item);
        state.addMember("modSourceID", item.processor->getSource().getID());
        return state;
    }

protected:
    std::shared_ptr<ModGenerator> createProcessorForType(const GeneratorType type, const int id) const override {
        const std::string uid = "mod-" + std::to_string(id);
        const std::string name = to_string(type) + " " + std::to_string(id+1);
        if (type == GeneratorType::LFO && synth) return std::make_shared<CurveLFOGenerator>(*synth, modMatrix, uid, name);
        if (type == GeneratorType::Macro) return std::make_shared<MacroGenerator>(modMatrix, uid, name);
        if (type == GeneratorType::EnvelopeFollower && envSources) {
            return std::make_shared<EnvelopeFollowerGenerator>(*envSources, modMatrix, uid, name);
        }
        if (type == GeneratorType::MIDI && midiSources) {
            return std::make_shared<MultichannelValueGenerator>(*midiSources, modMatrix, uid, name);
        }
        if (type == GeneratorType::Envelope && synth) {
            return std::make_shared<EnvelopeGenerator>(*synth, modMatrix, uid, name);
        }
        return nullptr;
    }

    void performTypeSpecificCleanup(const Item& item) override {
        item.processor->getSource().resetValue();
        item.processor->getSource().deregister();
    }

private:
    ModMatrix& modMatrix;
    EnvelopeFollowerSources* envSources {nullptr};
    MultichannelValueSources* midiSources {nullptr};
    MPESynth* synth {nullptr};
};