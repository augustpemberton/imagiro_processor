//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModMatrix.h"

namespace imagiro {
    class ModSource {
    public:
        explicit ModSource(std::string sourceName = "", ModMatrix* m = nullptr, bool bipolar = false)
                : name(std::move(sourceName)), matrix(m), bipolar(bipolar)
        {
            if (m) setModMatrix(*m);
        }

        void setModMatrix(ModMatrix& m) {
            matrix = &m;
            id = matrix->registerSource(name, bipolar);
        }

        void setBipolar(const bool bipolar) {
            this->bipolar = bipolar;
            matrix->updateSource(id, name, bipolar);
        }

        bool isBipolar() const { return bipolar; }

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
         * if bipolar, v is between -0.5 -> 0.5
         */
        void setVoiceValue(float v, size_t voiceIndex) {
            if (!matrix) {
                jassertfalse;
                return;
            }

            if (bipolar) {
                jassert(v <= 0.5 && v >= -0.5); // bipolar values are between -0.5 and 0.5!
                v = std::clamp(v, -0.5f, 0.5f);
            } else {
                v = std::clamp(v, 0.f, 1.f);
            }

            matrix->setVoiceSourceValue(id, voiceIndex, v);
        }

        void connectTo(const TargetID& targetID, ModMatrix::Connection::Settings connectionSettings) {
            if (!matrix) {
                jassertfalse;
                return;
            }

            matrix->queueConnection(id, targetID, connectionSettings);
        }

        SourceID getID() {
            return id;
        }

        const ModMatrix* getMatrix() const { return matrix; }

    private:
        SourceID id;
        std::string name;
        ModMatrix* matrix;

        bool bipolar;
    };
}