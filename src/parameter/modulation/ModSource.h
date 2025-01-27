//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModMatrix.h"

namespace imagiro {
    class ModSource {
    public:
        ModSource(ModMatrix::ModulationType type, ModMatrix* m) : matrix(m), modulationType(type) {
            if (m) setModMatrix(*m);
        }

        void setModMatrix(ModMatrix& m) {
            matrix = &m;
            id = matrix->registerSource(modulationType);
        }

        // global
        void setValue(float v) {
            if (!matrix || !id) {
                jassertfalse;
                return;
            }

            jassert (modulationType == ModMatrix::ModulationType::Global);
            matrix->setSourceValue(*id, v);
        }

        // per-voice
        void setValue(float v, size_t voiceIndex) {
            if (!matrix || !id) {
                jassertfalse;
                return;
            }

            jassert (modulationType == ModMatrix::ModulationType::PerVoice);
            matrix->setSourceValue(*id, voiceIndex, v);
        }

        void connectTo(TargetID targetID, ModMatrix::ConnectionInfo connection) {
            if (!matrix || !id) {
                jassertfalse;
                return;
            }

            matrix->setConnectionInfo(*id, targetID, connection);
        }

        ModMatrix::ModulationType getModulationType() const { return modulationType; }

        SourceID getID() {
            jassert(id);
            return *id;
        }

    private:
        std::optional<SourceID> id;
        ModMatrix* matrix;

        ModMatrix::ModulationType modulationType;
    };
}