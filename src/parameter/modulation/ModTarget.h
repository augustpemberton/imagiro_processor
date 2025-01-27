//
// Created by August Pemberton on 27/01/2025.
//

#pragma once
#include "ModMatrix.h"

namespace imagiro {
    class ModTarget {
    public:
        ModTarget(ModMatrix &m) : matrix(m) {
            id = matrix.registerTarget();
        }

        float getModulatedValue(float baseValue) {
            return baseValue + matrix.getModulatedValue(id);
        }

        void connectTo(ModSource source, ModMatrix::ConnectionInfo connection) {
            matrix.setConnectionInfo(source.getID(), id, connection);
        }

    private:
        TargetID id;

        ModMatrix &matrix;
    };
}