//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "ProcessorChainProcessor.h"
#include "chorus/ChorusProcessor.h"
#include "saturation/SaturationProcessor.h"
#include "gain/GainProcessor.h"
#include "iir-filter/IIRFilterProcessor.h"
#include "imagiro_processor/src/parameter/ProxyParameter.h"
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
    };

    static std::shared_ptr<Processor> getProcessorForEffectType(const EffectType type) {
        if (type == EffectType::Gain) return std::make_shared<GainProcessor>();
        if (type == EffectType::DiffuseDelay) return std::make_shared<DiffuseDelayProcessor>();
        if (type == EffectType::Filter) return std::make_shared<IIRFilterProcessor>();
        if (type == EffectType::Chorus) return std::make_shared<ChorusProcessor>();
        if (type == EffectType::Saturation) return std::make_shared<SaturationProcessor>();
        if (type == EffectType::Wobble) return std::make_shared<WobbleProcessor>();
        return nullptr;
    }

    struct Effect {
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
    void setChainTypes(const TypesList &list) {
        setChain(createChain(list));
    }

    void setChain(const EffectChain &chain) {
        if (proxyParameters) updateProxyParameters(chain);

        currentChain = chain;
        listeners.call(&Listener::OnChainUpdated);

        std::vector<std::shared_ptr<Processor>> processorList;
        for (auto& effect : currentChain) {
            processorList.push_back(effect.processor);
        }
        processorGraph.queueChain(processorList);
    }

    std::pair<Effect*, int> findProcessorOfTypeFromCurrentChain(EffectType type, int startIndex) {
        for (auto i = startIndex; i<currentChain.size(); i++) {
            if (currentChain[i].type == type) {
                return {&currentChain[i], i};
            }
        }
        return {nullptr, -1};
    }

    void updateProxyParameters(const EffectChain& chain) {
        for (auto& p : *proxyParameters) {
            p->clearProxyTarget();
        }

        auto oldChainPos = 0;
        auto it = proxyParameters->begin();
        auto oldMappedProxyParams = mappedProxyParameters;
        mappedProxyParameters.clear();

        for (auto i=0; i<chain.size(); i++) {
            auto& effect = chain[i];

            auto [oldEffect, oldEffectIndex] = findProcessorOfTypeFromCurrentChain(effect.type, oldChainPos);
            // if we found a matching effect, increase index, so we don't select it again
            if (oldEffect != nullptr) {
                oldChainPos = oldEffectIndex+1;
            }

            mappedProxyParameters[i] = std::map<std::string, ProxyParameter*>{};

            for (const auto& param : effect.processor->getPluginParameters()) {
                ProxyParameter* proxyParam;
                if (oldEffect) {
                    auto oldEffectParam = oldEffect->processor->getParameter(param->getUID());

                    // load old param state into new param
                    auto oldEffectParamState = oldEffectParam->getState();
                    param->setState(oldEffectParamState);

                    // map same proxy parameter to preserve automation
                    auto oldProxyParam = oldMappedProxyParams[oldEffectIndex][param->getUID()];
                    oldProxyParam->setProxyTarget(*param);
                    proxyParam = oldProxyParam;
                } else {
                    while (!proxyParam || proxyParam->isProxySet()) {
                        if (it == proxyParameters->end()) {
                            jassertfalse;
                            break;
                        }

                        proxyParam = *it;
                        ++it;
                    }
                }

                mappedProxyParameters[i][param->getUID()] = proxyParam;
                proxyParam->setProxyTarget(*param);
            }
        }
    }

    Processor& getProcessor() { return processorGraph; }
    const EffectChain& getCurrentChain() { return currentChain; }


    static EffectChain createChain(const TypesList &typesChain) {
        EffectChain chain;
        for (const auto& type : typesChain) {
            chain.push_back({
                type,
                getProcessorForEffectType(type)
            });
        }
        return chain;
    }

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
            chain.push_back({ effectType, processor });
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
};
