//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModMatrix.h"

namespace imagiro {
    class ModTarget {
    public:
        ModTarget(ModMatrix::ModulationType type, ModMatrix* m = nullptr) : modulationType(type) {
            if (m) setModMatrix(*m);
        }

        void setModMatrix(ModMatrix& m) {
            matrix = &m;
            id = matrix->registerTarget(modulationType);
        }

        float getModulatedValue(float baseValue = 0.f) const {
            if (!matrix || !id) {
                jassertfalse;
                return baseValue;
            }

            jassert (modulationType == ModMatrix::ModulationType::Global);
            return baseValue + matrix->getModulatedValue(*id);
        }

        float getModulatedValue(float baseValue, size_t voiceIndex) const {
            if (!matrix || !id) {
                jassertfalse;
                return baseValue;
            }

            jassert (modulationType == ModMatrix::ModulationType::PerVoice);
            return baseValue + matrix->getModulatedValue(*id, voiceIndex);
        }

        void connectTo(SourceID sourceID, ModMatrix::ConnectionInfo connection) {
            if (!matrix || !id) {
                jassertfalse;
                return;
            }

            matrix->setConnectionInfo(sourceID, *id, connection);
        }

        ModMatrix::ModulationType getModulationType() const { return modulationType; }

        TargetID getID() {
            jassert(id);
            return *id;
        }

    private:
        std::optional<TargetID> id;
        ModMatrix* matrix;

        ModMatrix::ModulationType modulationType;
    };
}