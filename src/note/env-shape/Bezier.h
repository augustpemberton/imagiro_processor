//
// Created by August Pemberton on 30/08/2023.
//

#pragma once
#include <juce_dsp/juce_dsp.h>

struct CurvePoint {
    float x;
    float y;
};

class Bezier {
public:
    Bezier(CurvePoint start_, CurvePoint end_, CurvePoint control_)
        : start(start_), end(end_), control(control_)
    { }

    float getY(float x) {
        if (x <= start.x) return start.x;
        if (x >= end.x) return end.x;
        auto t = juce::jmap(x, start.x, end.x, 0.f, 1.f);

        auto error = 1.f;
        auto dt = 0.25f;

        auto iter = 0;

        while (error > 0.01f && iter < 100) {
            t += dt;
            auto p = getCurvePoint(t);
            error = abs(p.x - x);
            if (abs(error) <= abs(dt) || t > 1.f) dt /=2;
            t = std::min(1.f, t);
            if (p.x-x > 0) dt = -abs(dt);
            else dt = abs(dt);
            iter++;
        }

        return getCurvePoint(t).y;
    }

    CurvePoint getCurvePoint(float t) {
        auto ta = getPt(start, control, t);
        auto tb = getPt(control, end, t);
        return getPt(ta, tb, t);
    }

    const CurvePoint start;
    const CurvePoint end;
    const CurvePoint control;

private:
    // get a point along a straight line
    static CurvePoint getPt(CurvePoint n1, CurvePoint n2, float t) {
        auto diff = CurvePoint{n2.x - n1.x, n2.y - n1.y};
        return {
            n1.x + diff.x * t,
            n1.y + diff.y * t};
    }
};
