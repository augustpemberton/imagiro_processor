//
// Created by August Pemberton on 09/06/2025.
//

#pragma once
#include "CurvePoint.h"
#include "juce_dsp/juce_dsp.h"
#include "./Bezier.h"

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
        return lookupTable->getUnchecked(x * lookupTable->getNumPoints() - 1);
    }

    std::shared_ptr<juce::dsp::LookupTable<float>> getLookupTable() const { return lookupTable; }

private:
    std::map<float, CurvePoint> points;
    std::shared_ptr<juce::dsp::LookupTable<float>> lookupTable;

    void reloadLookupTable(float numPoints = 1024) {
        lookupTable = std::make_shared<juce::dsp::LookupTable<float>>([&](const int n) -> float {
            const float p = n / numPoints;

            if (points.empty()) return 0.f;
            if (points.size() == 1) return points[0].y;

            auto it = points.lower_bound(p);
            if (it != points.begin()) it = std::prev(it);

            const auto& start = it->second;
            const auto nextIterator = std::next(it);
            if (nextIterator == points.end()) return start.y;

            const auto end = nextIterator->second;

            const Bezier bezier ({start.x, start.y}, {end.x, end.y}, {start.controlX, start.controlY});
            return bezier.getY(p) * 2 - 1;
        }, numPoints);
    }
};
