//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModMatrix.h"

namespace imagiro {
    class ModSource {
    public:
        ModSource(ModMatrix &m)
                : matrix(m) {
            id = matrix.registerSource();
        }

        void setValue(float v) {
            matrix.setSourceValue(id, v);
        }

        SourceID& getID() { return id; }
    private:
        SourceID id;
        ModMatrix &matrix;
    };
}