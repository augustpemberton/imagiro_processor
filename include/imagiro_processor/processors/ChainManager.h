//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "ProcessorChainProcessor.h"
#include "chorus/ChorusProcessor.h"
#include "imagiro_processor/src/parameter/ProxyParameter.h"

namespace imagiro {
    template<typename ItemType, typename ProcessorType>
    class ChainManager : juce::Timer {
    public:
        using ParameterFactory = std::function<ProxyParameter*(int)>;

        ChainManager(const ParameterFactory &parameterFactory, const int numSlots, const int maxParamsPerSlot, int numChannels = 2)
            : numSlots(numSlots), maxParamsPerSlot(maxParamsPerSlot), processorGraph(numChannels) {
            for (auto i=0; i<numSlots; i++) {
                proxyParameters.push_back(std::vector<ProxyParameter*>());
                for (auto j=0; j<maxParamsPerSlot; j++) {
                    proxyParameters[i].push_back(parameterFactory(i*maxParamsPerSlot + j));
                }
            }
            startTimer(1000);
        }

        ~ChainManager() override {
            stopTimer();
            resetAllProxies();
        }

        void resetProxies(int processorID) {
            for (const auto [uid, param] : mappedProxyParameters[processorID]) {
                param->clearProxyTarget();
            }
            mappedProxyParameters[processorID].clear();
        }

        void resetAllProxies() {
            for (const auto [processorID, map] : mappedProxyParameters) {
                resetProxies(processorID);
            }

            mappedProxyParameters.clear();
        }

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
            ProxyParameter* p;

            while (proxyParamsToUpdate.try_dequeue(p)) {
                p->processQueuedTarget();
            }

            processorGraph.processBlock(b, midi);
        }

        Item* getItem(int index) {
            if (index >= currentChain.size()) return nullptr;
            return &currentChain[index];
        }

        void setItem(size_t index, ItemType type, bool sync = false) {
            if (index >= numSlots) return;
            if (currentChain.size() > index && currentChain[index].type == type) return;
            auto newChain = currentChain;

            index = std::clamp(index, static_cast<size_t>(0), newChain.size());

            if (index == newChain.size()) newChain.push_back(Item{static_cast<int>(index), type});
            else newChain[index] = Item{static_cast<int>(index), type};
            setChain(newChain, sync);
        }

        /* Allocates - call from message thread */
        void setChain(Chain chain, const bool sync = false) {
            for (const auto& item : currentChain) {
                listeners.call(&Listener::OnItemRemoved, item);
            }

            updateProcessors(chain);

            auto oldChain = currentChain;
            currentChain = chain;
            allChains.push_back(currentChain);

            for (auto& item : currentChain) {
                listeners.call(&Listener::OnItemAdded, item);
            }

            std::vector<std::shared_ptr<Processor>> processorList;
            for (auto& item : currentChain) {
                processorList.push_back(item.processor);
            }

            if (sync) processorGraph.setChain(processorList);
            else processorGraph.queueChain(processorList);

            // cleanupOldItems(oldChain);
            listeners.call(&Listener::OnChainUpdated);
        }

        Processor& getProcessor() { return processorGraph; }
        const Chain& getCurrentChain() { return currentChain; }

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

        void loadState(const choc::value::ValueView& state, bool sync = false) {
            Chain chain;
            for (const auto& itemState : state) {
                auto itemType = static_cast<ItemType>(itemState["type"].getWithDefault(0));
                auto id = itemState["id"].getWithDefault(-1);
                auto processorState = Preset::fromState(itemState["ProcessorState"]);

                if (id >= numSlots) continue;

                Item item{id, itemType};
                item.processor = createNewProcessor(item.type, item.id);
                proxyMapItem(item);
                item.processor->loadPreset(processorState);
                chain.push_back(item);
            }

            setChain(chain, sync);
        }

        virtual choc::value::Value getItemState(const Item& item) {
            auto chainValue = choc::value::createObject("item");
            chainValue.setMember("type", static_cast<int>(item.type));
            chainValue.setMember("id", item.id);
            return chainValue;
        }

    protected:

        // Pure virtual methods to be implemented by derived classes
        virtual std::shared_ptr<ProcessorType> createProcessorForType(ItemType type, int id) const = 0;
        virtual void performTypeSpecificCleanup(const Item& item) {}

    private:
        ProcessorChainProcessor processorGraph;

        Chain currentChain;
        std::vector<Chain> allChains;

        std::vector<std::vector<ProxyParameter*>> proxyParameters;
        std::map<int, std::map<std::string, ProxyParameter*>> mappedProxyParameters;

        juce::ListenerList<Listener> listeners;

        void timerCallback() override {
            for (auto it = allChains.begin(); it != allChains.end();) {
                bool nonReferenced = true;
                for (const auto& item : *it) {
                    if (item.processor.use_count() > 1) {
                        nonReferenced = false;
                    }
                }

                if (nonReferenced) {
                    DBG("De-allocating chain");
                    cleanupOldItems(*it);
                    it = allChains.erase(it);
                } else ++it;
            }
        }

        void cleanupOldItems(const Chain& oldChain) {
            for (const auto& oldItem : oldChain) {
                auto oldProc = oldItem.processor.get();
                auto newVersion = std::ranges::find_if(currentChain,
                                                       [oldProc](const Item& item) {
                                                           return item.processor.get() == oldProc;
                                                       });

                // if the new version has that processor we aren't deleting it
                if (newVersion != currentChain.end()) {
                    continue;
                }

                // Perform type-specific cleanup
                performTypeSpecificCleanup(oldItem);
            }
        }

        void updateProcessors(Chain& chain) {
            resetAllProxies();

            for (auto& item : chain) {
                if (item.id < 0) {
                    for (int idx = 0; idx<numSlots; idx++) {
                        if (!findItemInChain(chain, idx)) {
                            item.id = idx;
                            break;
                        }
                    }
                    if (item.id < 0) {
                        jassertfalse; // too many items in the chain!
                        continue;
                    }
                }

                if (!item.processor) {
                    if (currentChain.size() > item.id && currentChain[item.id].type == item.type) {
                        item.processor = currentChain[item.id].processor;
                        currentChain[item.id].processor.reset();
                    } else {
                        item.processor = createNewProcessor(item.type, item.id);
                    }
                }

                proxyMapItem(item);
            }
        }

        std::shared_ptr<ProcessorType> createNewProcessor(const ItemType& type, const int id) {
            const auto processor = createProcessorForType(type, id);
            for (const auto& param : processor->getPluginParameters()) {
                param->setName(param->getName(200) + " " + std::to_string(id));
            }
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

            for (auto i=0; i<item.processor->getPluginParameters().size(); i++) {
                auto param = item.processor->getPluginParameters()[i];
                if (param->isInternal()) continue;
                auto proxyParam = proxyParameters[item.id][i];

                if (!proxyParam) {
                    jassertfalse;
                    break;
                }

                if (proxyParam->isProxySet()) {
                    jassertfalse;
                    proxyParam->clearProxyTarget();
                }

                proxyParam->queueProxyTarget(*param);
                proxyParamsToUpdate.enqueue(proxyParam);
                mappedProxyParameters[item.id][param->getUID()] = proxyParam;
            }
        }

        static Item* findItemInChain(Chain& chain, const int id) {
            for (auto& item : chain) {
                if (item.id == id) return &item;
            }
            return nullptr;
        }

        moodycamel::ReaderWriterQueue<ProxyParameter*> proxyParamsToUpdate{200};
        int numSlots;
        int maxParamsPerSlot;
    };
}