//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "GeneratorChainProcessor.h"
#include "imagiro_processor/src/parameter/ProxyParameter.h"
#include "LFO/LFOGenerator.h"
#include "Macro/MacroGenerator.h"

class GeneratorChainManager {
public:
    GeneratorChainManager(ModMatrix& m) : modMatrix(m) {

    }

    enum class GeneratorType {
        LFO = 0,
        Macro = 1
    };

    static std::string to_string(const GeneratorType type) {
        switch (type) {
            case GeneratorType::LFO:
                return "LFO";
            case GeneratorType::Macro:
                return "Macro";
        }
        return "";
    }

    std::shared_ptr<ModGenerator> getModGeneratorForGeneratorType(const GeneratorType type, int id) const {
        const std::string uid = "mod-" + std::to_string(id);
        const std::string name = to_string(type) + " " + std::to_string(id);
        if (type == GeneratorType::LFO) return std::make_shared<LFOGenerator>(modMatrix, uid, name);
        if (type == GeneratorType::Macro) return std::make_shared<MacroGenerator>(modMatrix, uid, name);
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

        std::vector<std::shared_ptr<ModGenerator>> processorList;
        for (auto& effect : currentChain) {
            processorList.push_back(effect.generator);
        }
        processor.queueChain(processorList);

        cleanupDeletedGenerators(oldChain);
        listeners.call(&Listener::OnChainUpdated);
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

            Generator g {id, effectType};
            createNewModGenerator(g, ++id);
            g.generator->loadPreset(processorState);
            chain.push_back(g);
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
            oldGenerator.generator->getSource().deregister();

            for (auto [uid, param] : mappedProxyParameters[id]) {
                param->getModTarget().clearConnections();
                param->getModTarget().deregister();
                param->clearProxyTarget();
            }
            mappedProxyParameters.erase(id);

        }
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
        newGenerator.generator = getModGeneratorForGeneratorType(oldGenerator.type, oldGenerator.id);
        for (auto& param : newGenerator.generator->getPluginParameters()) {
            auto oldParam = oldGenerator.generator->getParameter(param->getUID());
            param->setState(oldParam->getState());

            if (mappedProxyParameters[oldGenerator.id].contains(param->getUID())) {
                auto proxy = mappedProxyParameters[oldGenerator.id][param->getUID()];
                param->setModTarget(proxy->getModTarget());
                proxy->setProxyTarget(*param);
            }
        }
    }

    void createNewModGenerator(Generator& e, int id) {
        e.generator = getModGeneratorForGeneratorType(e.type, id);
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

            param->setModTarget(ModTarget("param-generator-"+std::to_string(e.id)+param->getUID()));
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
