//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include <utility>

#include "ModMatrix.h"

namespace imagiro {
    class ModTarget : ModMatrix::Listener {
    public:

        struct Listener {
            virtual void OnTargetUpdated() {}
        };
        void addListener(Listener* l) { listeners.add(l); }
        void removeListener(Listener* l) { listeners.remove(l); }

        ModTarget(std::string targetName = "", ModMatrix* m = nullptr)
                : name(std::move(targetName))
        {
            if (m) setModMatrix(*m);
        }

        ~ModTarget() override {
            if (matrix) matrix->removeListener(this);
        }

        ModTarget(const ModTarget& other) {
            id = other.id;
            name = other.name;
            matrix = other.matrix;
        }

        ModTarget& operator=(const ModTarget& other) {
            id = other.id;
            name = other.name;
            matrix = other.matrix;
            return *this;
        }

        void OnTargetValueUpdated(const TargetID &targetID) override {
            if (targetID != id) return;
            listeners.call(&Listener::OnTargetUpdated);
        }

        void setModMatrix(ModMatrix& m) {
            if (matrix) matrix->removeListener(this);
            matrix = &m;
            id = matrix->registerTarget(name);
            matrix->addListener(this);
        }

        void deregister() const {
            matrix->removeTarget(id);
        }

        void setTargetID(const TargetID &targetID) {
            id = targetID;
        }

        float getModulatedValue(float baseValue, int voiceIndex = -1) const {
            if (!matrix) return baseValue;
            return std::clamp(baseValue + matrix->getModulatedValue(id, voiceIndex), 0.f, 1.f);
        }

        void connectTo(SourceID sourceID, ModMatrix::Connection::Settings connectionSettings) {
            if (!matrix) {
                jassertfalse;
                return;
            }

            matrix->setConnection(std::move(sourceID), id, connectionSettings);
        }

        TargetID getID() {
            return id;
        }

    private:
        TargetID id;
        std::string name;
        ModMatrix* matrix {nullptr};

        juce::ListenerList<Listener> listeners;
    };
}