//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "ProcessorChainProcessor.h"
#include "chorus/ChorusProcessor.h"
#include "saturation/SaturationProcessor.h"
#include "utility/UtilityProcessor.h"
#include "iir-filter/IIRFilterProcessor.h"
#include "imagiro_processor/src/parameter/ProxyParameter.h"
#include "erosion/ErosionProcessor.h"
#include "noise/NoiseProcessor.h"
#include "wobble/WobbleProcessor.h"

class EffectChainProcessor {
public:
    enum class EffectType {
        Gain = 0,
        DiffuseDelay = 1,
        Filter = 2,
        Chorus = 3,
        Saturation= 4,
        Wobble = 5,
        Noise = 6,
        Erosion = 7,
    };

    static std::shared_ptr<Processor> getProcessorForEffectType(const EffectType type) {
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

    struct Effect {
        int id {-1};
        EffectType type;
        std::shared_ptr<Processor> processor;
    };

    using EffectChain = std::vector<Effect>;
    using TypesList = std::vector<EffectType>;

    struct Listener {
        virtual ~Listener() = default;
        virtual void OnChainUpdated() {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    /* Allocates - call from message thread */
    void setChain(EffectChain chain) {
        updateProcessors(chain);

        // hold onto existing processors until after we call cleanup old proxy params
        auto oldChain = currentChain;

        currentChain = chain;
        listeners.call(&Listener::OnChainUpdated);

        std::vector<std::shared_ptr<Processor>> processorList;
        for (auto& effect : currentChain) {
            processorList.push_back(effect.processor);
        }
        processorGraph.queueChain(processorList);

        cleanupOldProxyParams();
    }

    Processor& getProcessor() { return processorGraph; }
    const EffectChain& getCurrentChain() { return currentChain; }

    void setProxyParameters(std::vector<ProxyParameter*>& p) {
        this->proxyParameters = &p;
    }

    auto& getProxyParameterMap() { return mappedProxyParameters; }

    choc::value::Value getState() {
        auto states = choc::value::createEmptyArray();
        for (auto& effect : currentChain) {
            auto state = choc::value::createObject("EffectState");
            state.addMember("EffectType", static_cast<int>(effect.type));

            auto processorPreset = effect.processor->createPreset("", true);
            state.addMember("ProcessorState", processorPreset.getState());
            states.addArrayElement(state);
        }
        return states;
    }

    void loadState(const choc::value::ValueView& state) {
        EffectChain chain;
        for (const auto& effectState : state) {
            auto effectType = static_cast<EffectType>(effectState["EffectType"].getWithDefault(0));
            auto processorState = Preset::fromState(effectState["ProcessorState"]);

            auto processor = getProcessorForEffectType(effectType);
            processor->loadPreset(processorState);
            chain.push_back({-1, effectType, processor });
        }

        setChain(chain);
    }

private:
    ProcessorChainProcessor processorGraph;

    EffectChain currentChain;

    std::set<ProxyParameter*> usedProxyParams;
    std::vector<ProxyParameter*>* proxyParameters {nullptr};
    std::map<int, std::map<std::string, ProxyParameter*>> mappedProxyParameters;

    juce::ListenerList<Listener> listeners;

    void cleanupOldProxyParams() {
        for (auto it = mappedProxyParameters.begin(); it != mappedProxyParameters.end();) {
            if (isIDInChain(currentChain, it->first)) {
                ++it;
            } else {
                for (auto [id, param] : it->second) {
                    param->clearProxyTarget();
                }
                it = mappedProxyParameters.erase(it);
            }
        }
    }

    bool isIDInChain(const EffectChain& chain, int id) {
        for (const auto& effect : chain) {
            if (effect.id == id) return true;
        }
        return false;
    }

    std::pair<Effect*, int> findProcessorOfTypeFromCurrentChain(EffectType type, int startIndex) {
        for (auto i = startIndex; i<currentChain.size(); i++) {
            if (currentChain[i].type == type) {
                return {&currentChain[i], i};
            }
        }
        return {nullptr, -1};
    }

    static int getMaxIDInChain(const EffectChain &chain) {
        int maxID = 0;
        for (auto& effect : chain) {
            maxID = std::max(effect.id, maxID);
        }
        return maxID;
    }

    static Effect* findEffectInChain(EffectChain& chain, const int id) {
        for (auto& effect : chain) {
            if (effect.id == id) return &effect;
        }
        return nullptr;
    }

    void updateProcessors(EffectChain &chain) {
        for (auto& effect : chain) {
            if (effect.processor) continue;
            if (effect.id < 0) {
                createNewProcessor(effect, getMaxIDInChain(chain) + 1);
                continue;
            }

            auto currentEffect = findEffectInChain(currentChain, effect.id);
            if (currentEffect) copyProcessorAndMoveProxyParams(effect, *currentEffect);
            else createNewProcessor(effect, getMaxIDInChain(chain) + 1);
        }
    }

    void copyProcessorAndMoveProxyParams(Effect& newEffect, Effect& oldEffect) {
        newEffect.processor = getProcessorForEffectType(oldEffect.type);
        newEffect.id = oldEffect.id;
        for (auto& param : newEffect.processor->getPluginParameters()) {
            auto oldParam = oldEffect.processor->getParameter(param->getUID());
            param->setState(oldParam->getState());

            if (mappedProxyParameters[oldEffect.id].contains(param->getUID())) {
                auto proxy = mappedProxyParameters[oldEffect.id][param->getUID()];
                proxy->setProxyTarget(*param);
            }
        }
    }

    void createNewProcessor(Effect& e, int id) {
        e.processor = getProcessorForEffectType(e.type);
        e.id = id;
        proxyMapEffect(e);
    }

    void proxyMapEffect(Effect& e) {
        // cleanup existing params
        if (mappedProxyParameters.contains(e.id)) {
            auto existingProxyMap = mappedProxyParameters[e.id];
            for (auto [id, param] : existingProxyMap) {
                param->clearProxyTarget();
            }
            mappedProxyParameters[e.id] = std::map<std::string, ProxyParameter*>();
        }


        for (auto& param : e.processor->getPluginParameters()) {
            if (param->isInternal()) continue;
            auto proxyParam = getFreeProxyParameter();
            if (!proxyParam) {
                jassertfalse;
                break;
            }

            proxyParam->setProxyTarget(*param);
            mappedProxyParameters[e.id][param->getUID()] = proxyParam;
        }
    }

    ProxyParameter* getFreeProxyParameter() {
        if (!proxyParameters) return nullptr;
        for (auto param : *proxyParameters) {
            if (param->isProxySet()) continue;
            return param;
        }
        return nullptr;
    }
};
