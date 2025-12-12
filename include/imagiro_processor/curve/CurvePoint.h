//
// Created by August Pemberton on 09/06/2025.
//

#pragma once
#include "Point2D.h"

namespace imagiro {
    struct CurvePoint {
        Point2D position;
        float curve;

        explicit CurvePoint(const float x = 0.f, const float y = 0.f, const float curve = 0.f)
            : position(x, y), curve(curve) {
        }

        explicit CurvePoint(const Point2D pos, const float curve = 0.f)
            : position(pos), curve(curve) {
        }
    };
}
