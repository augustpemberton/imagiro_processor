//
// Created by August Pemberton on 30/08/2023.
//

#pragma once
#include <juce_dsp/juce_dsp.h>


class Bezier {
public:
    struct Point {
        float x;
        float y;
    };

    Bezier(const Point start_, const Point end_, const Point control_)
        : start(start_), end(end_), control(control_)
    { }

    float getY(const float x) const {
        if (x <= start.x) return start.x;
        if (x >= end.x) return end.x;
        auto t = juce::jmap(x, start.x, end.x, 0.f, 1.f);

        auto error = 1.f;
        auto dt = 0.2f;

        auto iter = 0;

        while (error > 0.001f && iter < 100) {
            t += dt;
            auto [px, py] = getPoint(t);
            error = abs(px - x);
            if (abs(error) <= abs(dt) || t > 1.f) dt /=2;
            t = std::min(1.f, t);
            if (px-x > 0) dt = -abs(dt);
            else dt = abs(dt);
            iter++;
        }

        return getPoint(t).y;
    }

    Point getPoint(const float t) const {
        const auto ta = getPt(start, control, t);
        const auto tb = getPt(control, end, t);
        return getPt(ta, tb, t);
    }

    const Point start;
    const Point end;
    const Point control;

private:
    // get a point along a straight line
    static Point getPt(const Point n1, const Point n2, const float t) {
        auto [x, y] = Point{n2.x - n1.x, n2.y - n1.y};
        return {
            n1.x + x * t,
            n1.y + y * t};
    }
};
