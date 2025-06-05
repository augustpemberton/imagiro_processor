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
        virtual void OnItemAdded(const Item& item) {}
        virtual void OnItemRemoved(const Item& item) {}
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

    Item* getItem(int id) {
        if (!itemsByID.contains(id)) return nullptr;
        return itemsByID[id];
    }

    /* Allocates - call from message thread */
    void setChain(Chain chain) {
        if (clearWaitingProxiesFlag) {
            proxiesWaitingToUpdate.clear();
            clearWaitingProxiesFlag = false;
        }

        for (const auto& item : currentChain) {
            listeners.call(&Listener::OnItemRemoved, item);
        }

        updateProcessors(chain);

        auto oldChain = currentChain;
        currentChain = chain;

        itemsByID.clear();
        for (auto& item : currentChain) {
            listeners.call(&Listener::OnItemAdded, item);
            itemsByID.insert({item.id, &item});
        }

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
            auto state = choc::value::createObject("ChainItem");
            state.addMember("type", static_cast<int>(item.type));
            state.addMember("id", item.id);

            auto processorPreset = item.processor->createPreset("", true);
            state.addMember("ProcessorState", processorPreset.getState());
            states.addArrayElement(state);
        }
        return states;
    }

    void loadState(const choc::value::ValueView& state) {
        Chain chain;
        for (const auto& itemState : state) {
            auto itemType = static_cast<ItemType>(itemState["type"].getWithDefault(0));
            auto id = itemState["id"].getWithDefault(-1);
            auto processorState = Preset::fromState(itemState["ProcessorState"]);

            Item item{id, itemType};
            createAndMapNewProcessor(item, id);
            item.processor->loadPreset(processorState);
            chain.push_back(item);
        }

        setChain(chain);
    }

    virtual std::string getPrefix() const = 0;

    virtual choc::value::Value getItemState(const Item& item) {
        auto chainValue = choc::value::createObject("item");
        chainValue.setMember("type", static_cast<int>(item.type));
        chainValue.setMember("id", item.id);
        return chainValue;
    }

protected:
    ChainManager(const unsigned int numChannels)
        : processorGraph(numChannels) {
    }

    // Pure virtual methods to be implemented by derived classes
    virtual std::shared_ptr<ProcessorType> createProcessorForType(ItemType type, int id) const = 0;
    virtual void performTypeSpecificCleanup(const Item& item) {}

private:
    ProcessorChainProcessor processorGraph;

    Chain currentChain;

    std::set<ProxyParameter*> usedProxyParams;
    std::vector<ProxyParameter*>* proxyParameters{nullptr};
    std::map<int, std::map<std::string, ProxyParameter*>> mappedProxyParameters;
    std::map<int, Item*> itemsByID;

    juce::ListenerList<Listener> listeners;

    void cleanupOldItems(const Chain& oldChain) {
        for (const auto& oldItem : oldChain) {
            auto id = oldItem.id;
            auto newVersion = std::ranges::find_if(currentChain,
                                                   [id](const Item& item) {
                                                       return item.id == id;
                                                   });

            // if the new version has that processor we aren't deleting it
            if (newVersion != currentChain.end()) continue;

            // Perform type-specific cleanup
            performTypeSpecificCleanup(oldItem);

            for (auto [uid, param] : mappedProxyParameters[oldItem.id]) {
                param->getModTarget().deregister();
                param->clearProxyTarget();
            }
            mappedProxyParameters.erase(oldItem.id);
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
                createAndMapNewProcessor(item, getMaxIDInChain(chain) + 1);
                continue;
            }

            if (auto currentItem = findItemInChain(currentChain, item.id)) {
                item.processor = currentItem->processor;
                currentItem->processor.reset();
            }
            else createAndMapNewProcessor(item, getMaxIDInChain(chain) + 1);
        }
    }

    void copyProcessorAndMoveProxyParams(Item& newItem, Item& oldItem) {
        newItem.processor = createNewProcessor(newItem.type, oldItem.id);
        newItem.id = oldItem.id;

        newItem.processor->loadPreset(oldItem.processor->createPreset("",true));
        for (auto& param : newItem.processor->getPluginParameters()) {
            auto oldParam = oldItem.processor->getParameter(param->getUID());
            // param->setState(oldParam->getState());

            if (mappedProxyParameters[oldItem.id].contains(param->getUID())) {
                auto proxy = mappedProxyParameters[oldItem.id][param->getUID()];
                param->setModTarget(proxy->getModTarget());
                proxy->queueProxyTarget(*param);
                proxyParamsToUpdate.enqueue(proxy);
                proxiesWaitingToUpdate.insert(proxy);
            }
        }
    }

    void createAndMapNewProcessor(Item& item, int id) {
        item.processor = createNewProcessor(item.type, id);
        item.id = id;
        proxyMapItem(item);
    }

    std::shared_ptr<ProcessorType> createNewProcessor(ItemType& type, int id) {
        const auto processor = createProcessorForType(type, id);
        return processor;
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

            param->setModTarget(ModTarget("param" + getPrefix() + std::to_string(item.id) + param->getUID()));
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
