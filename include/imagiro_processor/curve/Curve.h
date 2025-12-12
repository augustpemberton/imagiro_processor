#pragma once
#include "CurvePoint.h"
#include "juce_dsp/juce_dsp.h"

namespace imagiro {
    class Curve {
    public:
        explicit Curve(std::vector<CurvePoint> p = {}, const int numPoints = 256) {
            for (auto& point : p) {
                points.insert({point.position.x, point});
            }
            reloadLookupTable(numPoints);
        }

        float getY(const float x) const {
            if (points.size() == 0) return x;
            return lookupTable->getUnchecked(x * (lookupTable->getNumPoints() - 1));
        }

        std::shared_ptr<juce::dsp::LookupTable<float>> getLookupTable() const { return lookupTable; }

        void addPoint(const CurvePoint& point) {
            points.insert({point.position.x, point});
            reloadLookupTable();
        }

        void clear() {
            points.clear();
            reloadLookupTable();
        }

        size_t getPointCount() const { return points.size(); }

        const std::map<float, CurvePoint>& getPoints() const { return points; }

    private:
        std::map<float, CurvePoint> points;
        std::shared_ptr<juce::dsp::LookupTable<float>> lookupTable;

        // Power curve interpolation between two points
        static float powerInterpolate(const float t, const float y0, const float y1, const float curve) {
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
                if (points.size() == 1) return points.begin()->second.position.y;

                // Find the segment containing this x value
                const auto it = points.lower_bound(x);

                // If we're at or past the last point
                if (it == points.end()) {
                    return std::prev(it)->second.position.y;
                }

                // If we're at the first point
                if (it == points.begin()) {
                    return it->second.position.y;
                }

                // Get the previous point
                const auto prevIt = std::prev(it);
                const auto& start = prevIt->second;
                const auto& end = it->second;

                // Calculate normalized position within this segment
                const float segmentLength = end.position.x - start.position.x;
                if (segmentLength <= 0.0f) return start.position.y;

                float t = (x - start.position.x) / segmentLength;
                t = std::clamp(t, 0.0f, 1.0f);

                // Apply power curve interpolation
                const float y = powerInterpolate(t, start.position.y, end.position.y, start.curve);

                return y;
            }, numPoints);
        }
    };
}