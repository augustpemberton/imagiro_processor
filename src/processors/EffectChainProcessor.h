//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "ProcessorChainProcessor.h"
#include "chorus/ChorusProcessor.h"
#include "gain/GainProcessor.h"
#include "iir-filter/IIRFilterProcessor.h"
#include "imagiro_processor/src/parameter/ProxyParameter.h"

class EffectChainProcessor {
public:
    enum class EffectType {
        Gain = 0,
        DiffuseDelay = 1,
        Filter = 2,
        Chorus = 3
    };

    using EffectChain = std::list<EffectType>;

    /* Allocates - call from message thread */
    void setChain(const EffectChain &chain) {
        const auto c = createChain(chain);

        if (proxyParameters) updateProxyParameters(c);

        currentChain = chain;
        processorGraph.queueChain(c);
    }

    void updateProxyParameters(const ProcessorChainProcessor::ProcessorChain& chain) {
        for (auto& p : *proxyParameters) {
            p->clearProxyTarget();
        }

        auto it = proxyParameters->begin();

        auto chainIndex = 0;
        for (const auto& effect : chain) {
            mappedProxyParameters[chainIndex] = std::vector<ProxyParameter*>{};
            for (const auto& param : effect->getPluginParameters()) {
                if (it == proxyParameters->end()) {
                    jassertfalse;
                    break;
                }

                mappedProxyParameters[chainIndex].push_back(*it);
                (*it)->setProxyTarget(*param);
                ++it;
            }
            chainIndex++;
        }
    }

    Processor& getProcessor() { return processorGraph; }
    const EffectChain& getCurrentChain() { return currentChain; }

    static std::shared_ptr<Processor> getProcessorForEffectType(const EffectType type) {
        if (type == EffectType::Gain) return std::make_shared<GainProcessor>();
        if (type == EffectType::DiffuseDelay) return std::make_shared<DiffuseDelayProcessor>();
        if (type == EffectType::Filter) return std::make_shared<IIRFilterProcessor>();
        if (type == EffectType::Chorus) return std::make_shared<ChorusProcessor>();
        return nullptr;
    }

    static ProcessorChainProcessor::ProcessorChain createChain(const EffectChain &typesChain) {
        ProcessorChainProcessor::ProcessorChain chain;
        for (const auto& type : typesChain) {
            chain.push_back(getProcessorForEffectType(type));
        }
        return chain;
    }

    void setProxyParameters(std::vector<ProxyParameter*>& p) {
        this->proxyParameters = &p;
    }

    auto& getProxyParameterMap() { return mappedProxyParameters; }

private:
    ProcessorChainProcessor processorGraph;

    EffectChain currentChain;

    std::vector<ProxyParameter*>* proxyParameters {nullptr};
    std::map<int, std::vector<ProxyParameter*>> mappedProxyParameters;
};
