//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include <utility>

#include "ModMatrix.h"

namespace imagiro {
    class ModTarget {
    public:
        ModTarget(TargetID targetID, std::string targetName = "", ModMatrix* m = nullptr)
                : id(std::move(targetID)), name(std::move(targetName))
        {
            if (m) setModMatrix(*m);
        }

        void setModMatrix(ModMatrix& m) {
            matrix = &m;
            matrix->registerTarget(id, name);
        }

        void clearConnections() const {
            matrix->removeConnectionsWithTarget(id);
        }

        void setTargetID(const TargetID &targetID) {
            id = targetID;
        }

        float getModulatedValue(float baseValue, int voiceIndex = -1) const {
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
        ModMatrix* matrix;
    };
}