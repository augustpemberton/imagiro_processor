//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModMatrix.h"

namespace imagiro {
    class ModTarget {
    public:
        ModTarget(std::string targetName = "", ModMatrix* m = nullptr)
                : name(targetName)
        {
            if (m) setModMatrix(*m);
        }

        void setModMatrix(ModMatrix& m) {
            matrix = &m;
            id = matrix->registerTarget(name);
        }

        float getModulatedValue(float baseValue, int voiceIndex = -1) const {
            return std::clamp(baseValue + matrix->getModulatedValue(*id, voiceIndex), 0.f, 1.f);
        }

        void connectTo(SourceID sourceID, ModMatrix::ConnectionInfo connection) {
            if (!matrix || !id) {
                jassertfalse;
                return;
            }

            matrix->setConnectionInfo(sourceID, *id, connection);
        }

        TargetID getID() {
            jassert(id);
            return *id;
        }

        int getNumModSources() {
            if (!matrix || !id) {
                jassertfalse;
                return 0;
            }

            return matrix->getNumModSources(*id);
        }

    private:
        std::optional<TargetID> id;
        std::string name;
        ModMatrix* matrix;
    };
}