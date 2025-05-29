//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "GeneratorChainProcessor.h"
#include "imagiro_processor/src/parameter/ProxyParameter.h"
#include "LFO/LFOGenerator.h"

class GeneratorChainManager {
public:
    GeneratorChainManager(ModMatrix& m) : modMatrix(m) {

    }

    enum class GeneratorType {
        LFO = 0
    };

    std::shared_ptr<ModGenerator> getModGeneratorForGeneratorType(const GeneratorType type, std::string uid, std::string name) const {
        if (type == GeneratorType::LFO) return std::make_shared<LFOGenerator>(modMatrix, uid, name);
        return nullptr;
    }

    struct Generator {
        int id {-1};
        GeneratorType type;
        std::shared_ptr<ModGenerator> generator;
    };

    using GeneratorChain = std::vector<Generator>;
    using TypesList = std::vector<GeneratorType>;

    struct Listener {
        virtual ~Listener() = default;
        virtual void OnChainUpdated() {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    /* Allocates - call from message thread */
    void setChain(GeneratorChain chain) {
        updateModGenerators(chain);

        // hold onto existing processors until after we call cleanup old proxy params
        auto oldChain = currentChain;

        currentChain = chain;
        listeners.call(&Listener::OnChainUpdated);

        std::vector<std::shared_ptr<ModGenerator>> processorList;
        for (auto& effect : currentChain) {
            processorList.push_back(effect.generator);
        }
        processor.queueChain(processorList);

        cleanupDeletedGenerators(oldChain);
    }

    Processor& getProcessor() { return processor; }
    const GeneratorChain& getCurrentChain() { return currentChain; }

    void setProxyParameters(std::vector<ProxyParameter*>& p) {
        this->proxyParameters = &p;
    }

    auto& getProxyParameterMap() { return mappedProxyParameters; }

    choc::value::Value getState() {
        auto states = choc::value::createEmptyArray();
        for (auto& effect : currentChain) {
            auto state = choc::value::createObject("GeneratorState");
            state.addMember("GeneratorType", static_cast<int>(effect.type));

            auto processorPreset = effect.generator->createPreset("", true);
            state.addMember("ModGeneratorState", processorPreset.getState());
            states.addArrayElement(state);
        }
        return states;
    }

    void loadState(const choc::value::ValueView& state) {
        GeneratorChain chain;
        int id=0;
        for (const auto& effectState : state) {
            auto effectType = static_cast<GeneratorType>(effectState["GeneratorType"].getWithDefault(0));
            auto processorState = Preset::fromState(effectState["ModGeneratorState"]);

            const std::string uid = "mod-" + std::to_string(id);
            const std::string name = "Modulator " + std::to_string(id);
            auto processor = getModGeneratorForGeneratorType(effectType, uid, name);
            processor->loadPreset(processorState);
            chain.push_back({-1, effectType, processor });
        }

        setChain(chain);
    }

private:
    GeneratorChainProcessor processor;
    ModMatrix& modMatrix;

    GeneratorChain currentChain;

    std::set<ProxyParameter*> usedProxyParams;
    std::vector<ProxyParameter*>* proxyParameters {nullptr};
    std::map<int, std::map<std::string, ProxyParameter*>> mappedProxyParameters;

    juce::ListenerList<Listener> listeners;

    void cleanupDeletedGenerators(const GeneratorChain& oldChain) {
        for (const auto& oldGenerator : oldChain) {
            auto id = oldGenerator.id;
            auto newVersion = std::ranges::find_if(currentChain,
                                                   [id](const Generator &g) {
                                                       return g.id == id;
                                                   });

            // if the new version has that id we aren't deleting it
            if (newVersion != currentChain.end()) continue;

            // else cleanup old generator

            oldGenerator.generator->getSource().resetValue();
            oldGenerator.generator->getSource().clearConnections();

            for (auto [uid, param] : mappedProxyParameters[id]) {
                param->getModTarget().clearConnections();
                param->clearProxyTarget();
            }
            mappedProxyParameters.erase(id);

        }
    }

    void cleanupGenerator(Generator& g) {

    }

    bool isIDInChain(const GeneratorChain& chain, int id) {
        for (const auto& effect : chain) {
            if (effect.id == id) return true;
        }
        return false;
    }

    std::pair<Generator*, int> findModGeneratorOfTypeFromCurrentChain(GeneratorType type, int startIndex) {
        for (auto i = startIndex; i<currentChain.size(); i++) {
            if (currentChain[i].type == type) {
                return {&currentChain[i], i};
            }
        }
        return {nullptr, -1};
    }

    static int getMaxIDInChain(const GeneratorChain &chain) {
        int maxID = 0;
        for (auto& effect : chain) {
            maxID = std::max(effect.id, maxID);
        }
        return maxID;
    }

    static Generator* findGeneratorInChain(GeneratorChain& chain, const int id) {
        for (auto& effect : chain) {
            if (effect.id == id) return &effect;
        }
        return nullptr;
    }

    void updateModGenerators(GeneratorChain &chain) {
        for (auto& effect : chain) {
            if (effect.generator) continue;
            if (effect.id < 0) {
                createNewModGenerator(effect, getMaxIDInChain(chain) + 1);
                continue;
            }

            auto currentGenerator = findGeneratorInChain(currentChain, effect.id);
            if (currentGenerator) copyModGeneratorAndMoveProxyParams(effect, *currentGenerator);
            else createNewModGenerator(effect, getMaxIDInChain(chain) + 1);
        }
    }

    void copyModGeneratorAndMoveProxyParams(Generator& newGenerator, Generator& oldGenerator) {
        newGenerator.id = oldGenerator.id;
        const std::string uid = "mod-" + std::to_string(oldGenerator.id);
        const std::string name = "Modulator " + std::to_string(oldGenerator.id);
        newGenerator.generator = getModGeneratorForGeneratorType(oldGenerator.type, uid, name);
        for (auto& param : newGenerator.generator->getPluginParameters()) {
            auto oldParam = oldGenerator.generator->getParameter(param->getUID());
            param->setState(oldParam->getState());

            if (mappedProxyParameters[oldGenerator.id].contains(param->getUID())) {
                auto proxy = mappedProxyParameters[oldGenerator.id][param->getUID()];
                proxy->setProxyTarget(*param);
            }
        }
    }

    void createNewModGenerator(Generator& e, int id) {
        const std::string uid = "mod-" + std::to_string(id);
        const std::string name = "Modulator " + std::to_string(id);
        e.generator = getModGeneratorForGeneratorType(e.type, uid, name);
        e.id = id;
        proxyMapGenerator(e);
    }

    void proxyMapGenerator(Generator& e) {
        // cleanup existing params
        if (mappedProxyParameters.contains(e.id)) {
            auto existingProxyMap = mappedProxyParameters[e.id];
            for (auto [id, param] : existingProxyMap) {
                param->clearProxyTarget();
            }
            mappedProxyParameters[e.id] = std::map<std::string, ProxyParameter*>();
        }


        for (auto& param : e.generator->getPluginParameters()) {
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

    ProxyParameter* getFreeProxyParameter() const {
        if (!proxyParameters) return nullptr;
        for (auto param : *proxyParameters) {
            if (param->isProxySet()) continue;
            return param;
        }
        return nullptr;
    }


};
