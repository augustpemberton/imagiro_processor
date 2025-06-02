//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "ProcessorChainProcessor.h"
#include "chorus/ChorusProcessor.h"
#include "imagiro_processor/src/parameter/ProxyParameter.h"

template<typename ItemType, typename ProcessorType>
class ChainManager {
public:
    virtual ~ChainManager() = default;

    struct Item {
        int id{-1};
        ItemType type;
        std::shared_ptr<ProcessorType> processor;
    };

    using Type = ItemType;
    using Chain = std::vector<Item>;
    using TypesList = std::vector<ItemType>;

    struct Listener {
        virtual ~Listener() = default;
        virtual void OnChainUpdated() {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    void processBlock(juce::AudioSampleBuffer& b, juce::MidiBuffer& midi) {
        processorGraph.processBlock(b, midi);
        ProxyParameter* p;

        bool updated = false;
        while (proxyParamsToUpdate.try_dequeue(p)) {
            p->processQueuedTarget();
            updated = true;
        }

        if (updated) clearWaitingProxiesFlag = true;
    }

    /* Allocates - call from message thread */
    void setChain(Chain chain) {
        if (clearWaitingProxiesFlag) {
            proxiesWaitingToUpdate.clear();
            clearWaitingProxiesFlag = false;
        }
        updateProcessors(chain);

        // hold onto existing processors until after we call cleanup old proxy params
        auto oldChain = currentChain;

        currentChain = chain;

        std::vector<std::shared_ptr<Processor>> processorList;
        for (auto& item : currentChain) {
            processorList.push_back(item.processor);
        }
        processorGraph.queueChain(processorList);

        cleanupOldItems(oldChain);
        listeners.call(&Listener::OnChainUpdated);
    }

    Processor& getProcessor() { return processorGraph; }
    const Chain& getCurrentChain() { return currentChain; }

    void setProxyParameters(std::vector<ProxyParameter*>& p) {
        this->proxyParameters = &p;
    }

    auto& getProxyParameterMap() { return mappedProxyParameters; }

    choc::value::Value getState() {
        auto states = choc::value::createEmptyArray();
        for (auto& item : currentChain) {
            auto state = choc::value::createObject(getStateObjectName());
            state.addMember(getTypeFieldName(), static_cast<int>(item.type));

            auto processorPreset = item.processor->createPreset("", true);
            state.addMember(getProcessorStateFieldName(), processorPreset.getState());
            states.addArrayElement(state);
        }
        return states;
    }

    void loadState(const choc::value::ValueView& state) {
        Chain chain;
        int id = 0;
        for (const auto& itemState : state) {
            auto itemType = static_cast<ItemType>(itemState[getTypeFieldName()].getWithDefault(0));
            auto processorState = Preset::fromState(itemState[getProcessorStateFieldName()]);

            Item item{id, itemType};
            createNewProcessor(item, ++id);
            item.processor->loadPreset(processorState);
            chain.push_back(item);
        }

        setChain(chain);
    }

protected:
    // Pure virtual methods to be implemented by derived classes
    virtual std::shared_ptr<ProcessorType> createProcessorForType(ItemType type, int id) const = 0;
    virtual std::string getModTargetPrefix() const = 0;
    virtual std::string getStateObjectName() const = 0;
    virtual std::string getTypeFieldName() const = 0;
    virtual std::string getProcessorStateFieldName() const = 0;
    virtual void performTypeSpecificCleanup(const Item& item) {}
    virtual bool shouldClearWaitingProxiesOnSetChain() const { return false; }
    virtual bool shouldPassInputToOutput() const { return true; }
    virtual int getChannelCount() const { return 0; }

    bool fadeBetweenChains;

    ChainManager(const bool fadeBetweenChains, const unsigned int numChannels)
        : fadeBetweenChains(fadeBetweenChains), processorGraph(fadeBetweenChains, numChannels) {
    }

private:
    ProcessorChainProcessor processorGraph;

    Chain currentChain;

    std::set<ProxyParameter*> usedProxyParams;
    std::vector<ProxyParameter*>* proxyParameters{nullptr};
    std::map<int, std::map<std::string, ProxyParameter*>> mappedProxyParameters;

    juce::ListenerList<Listener> listeners;

    void cleanupOldItems(const Chain& oldChain) {
        for (const auto& oldItem : oldChain) {
            auto id = oldItem.id;
            auto newVersion = std::ranges::find_if(currentChain,
                                                   [id](const Item& item) {
                                                       return item.id == id;
                                                   });

            // if the new version has that id we aren't deleting it
            if (newVersion != currentChain.end()) continue;

            // Perform type-specific cleanup
            performTypeSpecificCleanup(oldItem);

            for (auto [uid, param] : mappedProxyParameters[id]) {
                param->getModTarget().deregister();
                param->clearProxyTarget();
            }
            mappedProxyParameters.erase(id);
        }
    }

    bool isIDInChain(const Chain& chain, int id) {
        for (const auto& item : chain) {
            if (item.id == id) return true;
        }
        return false;
    }

    std::pair<Item*, int> findProcessorOfTypeFromCurrentChain(ItemType type, const int startIndex) {
        for (auto i = startIndex; i < currentChain.size(); i++) {
            if (currentChain[i].type == type) {
                return {&currentChain[i], i};
            }
        }
        return {nullptr, -1};
    }

    static int getMaxIDInChain(const Chain& chain) {
        int maxID = 0;
        for (auto& item : chain) {
            maxID = std::max(item.id, maxID);
        }
        return maxID;
    }

    static Item* findItemInChain(Chain& chain, const int id) {
        for (auto& item : chain) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }

    void updateProcessors(Chain& chain) {
        for (auto& item : chain) {
            if (item.processor) continue;
            if (item.id < 0) {
                createNewProcessor(item, getMaxIDInChain(chain) + 1);
                continue;
            }

            auto currentItem = findItemInChain(currentChain, item.id);
            if (currentItem) {
                if (fadeBetweenChains) copyProcessorAndMoveProxyParams(item, *currentItem);
                else {
                    item.processor = currentItem->processor;
                    currentItem->processor.reset();
                }
            }
            else createNewProcessor(item, getMaxIDInChain(chain) + 1);
        }
    }

    void copyProcessorAndMoveProxyParams(Item& newItem, Item& oldItem) {
        newItem.processor = createProcessorForType(oldItem.type, oldItem.id);
        newItem.id = oldItem.id;
        for (auto& param : newItem.processor->getPluginParameters()) {
            auto oldParam = oldItem.processor->getParameter(param->getUID());
            param->setState(oldParam->getState());

            if (mappedProxyParameters[oldItem.id].contains(param->getUID())) {
                auto proxy = mappedProxyParameters[oldItem.id][param->getUID()];
                param->setModTarget(proxy->getModTarget());
                proxy->queueProxyTarget(*param);
                proxyParamsToUpdate.enqueue(proxy);
                proxiesWaitingToUpdate.insert(proxy);
            }
        }
    }

    void createNewProcessor(Item& item, int id) {
        item.processor = createProcessorForType(item.type, id);
        item.id = id;
        proxyMapItem(item);
    }

    void proxyMapItem(Item& item) {
        // cleanup existing params
        if (mappedProxyParameters.contains(item.id)) {
            auto existingProxyMap = mappedProxyParameters[item.id];
            for (auto [id, param] : existingProxyMap) {
                param->clearProxyTarget();
            }
            mappedProxyParameters[item.id] = std::map<std::string, ProxyParameter*>();
        }

        for (auto& param : item.processor->getPluginParameters()) {
            if (param->isInternal()) continue;
            auto proxyParam = getFreeProxyParameter();
            if (!proxyParam) {
                jassertfalse;
                break;
            }

            param->setModTarget(ModTarget(getModTargetPrefix() + std::to_string(item.id) + param->getUID()));
            proxyParam->queueProxyTarget(*param);
            proxyParamsToUpdate.enqueue(proxyParam);
            proxiesWaitingToUpdate.insert(proxyParam);
            mappedProxyParameters[item.id][param->getUID()] = proxyParam;
        }
    }

    ProxyParameter* getFreeProxyParameter() const {
        if (!proxyParameters) return nullptr;
        for (auto param : *proxyParameters) {
            if (param->isProxySet()) continue;
            if (proxiesWaitingToUpdate.contains(param)) continue;
            return param;
        }
        return nullptr;
    }

    moodycamel::ReaderWriterQueue<ProxyParameter*> proxyParamsToUpdate{200};
    std::unordered_set<ProxyParameter*> proxiesWaitingToUpdate;
    std::atomic<bool> clearWaitingProxiesFlag{false};
};
