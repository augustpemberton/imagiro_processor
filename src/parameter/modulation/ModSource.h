//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModMatrix.h"

namespace imagiro {
    class ModSource {
    public:
        ModSource(SourceID sourceID, std::string sourceName = "", ModMatrix* m = nullptr,
                  ModMatrix::SourceType sourceType = ModMatrix::SourceType::Misc,
                  bool bipolar = false)
                : id(std::move(sourceID)), name(std::move(sourceName)), matrix(m), isBipolar(bipolar),
                  type (sourceType)
        {
            if (m) setModMatrix(*m);
        }

        void setModMatrix(ModMatrix& m) {
            matrix = &m;
            matrix->registerSource(id, name, type, isBipolar);
        }

        void setBipolar(bool bipolar) {
            isBipolar = bipolar;
            matrix->registerSource(id, name, type, isBipolar);
        }

        void resetValue() const {
            matrix->resetSourceValue(id);
        }

        void deregister() const {
            matrix->removeSource(id);
        }

        // global
        void setGlobalValue(float v) const {
            if (!matrix) {
                jassertfalse;
                return;
            }

            matrix->setGlobalSourceValue(id, v);
        }

        /*
         * if unipolar, v is between 0 -> 1
         * if bipolar, v is between -1 -> 1
         */
        void setVoiceValue(float v, size_t voiceIndex) {
            if (!matrix) {
                jassertfalse;
                return;
            }
            if (isBipolar) v = std::clamp(v, -1.f, 1.f);
            else v = std::clamp(v, 0.f, 1.f);

            matrix->setVoiceSourceValue(id, voiceIndex, v);
        }

        void connectTo(const TargetID& targetID, ModMatrix::Connection::Settings connectionSettings) {
            if (!matrix) {
                jassertfalse;
                return;
            }

            matrix->setConnection(id, targetID, connectionSettings);
        }

        SourceID getID() {
            return id;
        }

        const ModMatrix* getMatrix() const { return matrix; }

    private:
        SourceID id;
        std::string name;
        ModMatrix* matrix;
        bool isBipolar;
        ModMatrix::SourceType type;
    };
}