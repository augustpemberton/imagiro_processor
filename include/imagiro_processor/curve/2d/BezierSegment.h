//
// Created by August Pemberton on 18/07/2025.
//

#pragma once
#include <cmath>
#include "../Point2D.h"

namespace imagiro {
    struct BezierSegment {
        Point2D start;
        Point2D control1;
        Point2D control2;
        Point2D end;

        BezierSegment(const Point2D s, const Point2D c1, const Point2D c2, const Point2D e)
            : start(s), control1(c1), control2(c2), end(e) {}

        // Evaluate cubic bezier at parameter t (0.0 to 1.0)
        Point2D evaluate(const float t) const {
            const float u = 1.0f - t;
            const float tt = t * t;
            const float uu = u * u;
            const float uuu = uu * u;
            const float ttt = tt * t;

            Point2D p = start * uuu;
            p = p + control1 * (3.0f * uu * t);
            p = p + control2 * (3.0f * u * tt);
            p = p + end * ttt;

            return p;
        }

        // Approximate arc length using adaptive subdivision
        float getLength() const {
            return getLengthRecursive(0.0f, 1.0f, start, end, 0.01f);
        }

    private:
        float getLengthRecursive(const float t0, const float t1,
            const Point2D p0, const Point2D p1, const float tolerance) const {
            const float tMid = (t0 + t1) * 0.5f;
            const Point2D pmid = evaluate(tMid);

            const float d1 = distance(p0, p1);
            const float d2 = distance(p0, pmid) + distance(pmid, p1);

            if (std::abs(d2 - d1) < tolerance) {
                return d2;
            }

            return getLengthRecursive(t0, tMid, p0, pmid, tolerance) +
                   getLengthRecursive(tMid, t1, pmid, p1, tolerance);
        }

        static float distance(const Point2D& a, const Point2D& b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        }
    };
}