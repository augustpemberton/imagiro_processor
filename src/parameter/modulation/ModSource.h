//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModMatrix.h"

namespace imagiro {
    class ModSource {
    public:
        ModSource(ModMatrix* m) : matrix(m) {
            if (m) setModMatrix(*m);
        }

        void setModMatrix(ModMatrix& m) {
            matrix = &m;
            id = matrix->registerSource();
        }

        // global
        void setGlobalValue(float v) {
            if (!matrix || !id) {
                jassertfalse;
                return;
            }

            matrix->setGlobalSourceValue(*id, v);
        }

        // per-voice
        void setVoiceValue(float v, size_t voiceIndex) {
            if (!matrix || !id) {
                jassertfalse;
                return;
            }

            matrix->setVoiceSourceValue(*id, voiceIndex, v);
        }

        void connectTo(TargetID targetID, ModMatrix::ConnectionInfo connection) {
            if (!matrix || !id) {
                jassertfalse;
                return;
            }

            matrix->setConnectionInfo(*id, targetID, connection);
        }

        SourceID getID() {
            jassert(id);
            return *id;
        }

    private:
        std::optional<SourceID> id;
        ModMatrix* matrix;
    };
}