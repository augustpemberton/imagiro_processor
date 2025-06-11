//
// Created by August Pemberton on 09/06/2025.
//

#pragma once
#include "CurvePoint.h"
#include "juce_dsp/juce_dsp.h"

class Curve {
public:
    Curve(std::vector<CurvePoint> p = {}) {
        for (auto& point : p) {
            points.insert({point.x, point});
        }
        reloadLookupTable();
    }

    static Curve fromState(const choc::value::ValueView &state) {
        Curve c;
        for (const auto& pointState : state) {
            const auto point = CurvePoint::fromState(pointState);
            c.points.insert({point.x, point});
        }
        c.reloadLookupTable();
        return c;
    }

    choc::value::Value getState() const {
        auto state = choc::value::createEmptyArray();
        for (const auto& [start, point] : points) {
            state.addArrayElement(point.getState());
        }
        return state;
    }

    float getY(const float x) const {
        return lookupTable->getUnchecked(x * (lookupTable->getNumPoints() - 1));
    }

    std::shared_ptr<juce::dsp::LookupTable<float>> getLookupTable() const { return lookupTable; }

private:
    std::map<float, CurvePoint> points;
    std::shared_ptr<juce::dsp::LookupTable<float>> lookupTable;

    // Power curve interpolation between two points
    static float powerInterpolate(float t, float y0, float y1, float curve) {
        float easedT;

        const auto exponent = std::pow(50, curve);

        if (curve < 0) {
            easedT = std::pow(t, 1/exponent);
        } else if (curve > 0) {
            easedT = 1 - std::pow(1 - t, exponent);
        } else {
            easedT = t;
        }

        return y0 + (y1 - y0) * easedT;
    }

    void reloadLookupTable(float numPoints = 256) {
        lookupTable = std::make_shared<juce::dsp::LookupTable<float>>([&](const int n) -> float {
            const float x = n / numPoints;

            if (points.empty()) return 0.f;
            if (points.size() == 1) return points.begin()->second.y;

            // Find the segment containing this x value
            auto it = points.lower_bound(x);

            // If we're at or past the last point
            if (it == points.end()) {
                return std::prev(it)->second.y;
            }

            // If we're at the first point
            if (it == points.begin()) {
                return it->second.y;
            }

            // Get the previous point
            auto prevIt = std::prev(it);
            const auto& start = prevIt->second;
            const auto& end = it->second;

            // Calculate normalized position within this segment
            float segmentLength = end.x - start.x;
            if (segmentLength <= 0.0f) return start.y;

            float t = (x - start.x) / segmentLength;
            t = std::clamp(t, 0.0f, 1.0f);

            // Apply power curve interpolation
            float y = powerInterpolate(t, start.y, end.y, start.curve);

            return y;
        }, numPoints);
    }
};