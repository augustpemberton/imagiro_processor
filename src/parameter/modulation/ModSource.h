//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModMatrix.h"

namespace imagiro {
    class ModSource {
    public:
        ModSource(SourceID sourceID, std::string sourceName = "", ModMatrix* m = nullptr)
            : id(std::move(sourceID)), name(std::move(sourceName)), matrix(m)
        {
            if (m) setModMatrix(*m);
        }

        void setModMatrix(ModMatrix& m) {
            matrix = &m;
            matrix->registerSource(id, name);
        }

        // global
        void setGlobalValue(float v) {
            if (!matrix) {
                jassertfalse;
                return;
            }

            matrix->setGlobalSourceValue(id, v);
        }

        // per-voice
        void setVoiceValue(float v, size_t voiceIndex) {
            if (!matrix) {
                jassertfalse;
                return;
            }

            matrix->setVoiceSourceValue(id, voiceIndex, v);
        }

        void connectTo(TargetID targetID, ModMatrix::Connection::Settings connectionSettings) {
            if (!matrix) {
                jassertfalse;
                return;
            }

            matrix->setConnection(id, targetID, connectionSettings);
        }

        SourceID getID() {
            return id;
        }

    private:
        SourceID id;
        std::string name;
        ModMatrix* matrix;
    };
}