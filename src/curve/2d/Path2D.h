//
// Created by August Pemberton on 18/07/2025.
//

#pragma once
#include <vector>
#include "BezierSegment.h"
#include "juce_core/juce_core.h"

namespace imagiro {
    class Path2D {
    public:
        Path2D() = default;

        explicit Path2D(const std::vector<BezierSegment>& segments) : segments(segments) {
            calculateLengths();
        }

        void addSegment(const BezierSegment& segment) {
            segments.push_back(segment);
            calculateLengths();
        }

        void clear() {
            segments.clear();
            segmentLengths.clear();
            totalLength = 0.0f;
        }

        // Get point at percentage through path (0.0 to 1.0)
        Point2D getPointAt(float percentage) const {
            if (segments.empty()) return Point2D(0.0f, 0.0f);

            percentage = std::clamp(percentage, 0.0f, 1.0f);

            if (percentage == 0.0f) return segments[0].start;
            if (percentage == 1.0f) return segments.back().end;

            const float targetLength = percentage * totalLength;
            float currentLength = 0.0f;

            for (size_t i = 0; i < segments.size(); ++i) {
                const float segmentLength = segmentLengths[i];

                if (currentLength + segmentLength >= targetLength) {
                    // We're in this segment
                    const float segmentProgress = (targetLength - currentLength) / segmentLength;
                    return segments[i].evaluate(segmentProgress);
                }

                currentLength += segmentLength;
            }

            // Fallback (shouldn't reach here)
            jassertfalse;
            return segments.back().end;
        }

        // Get point using segment-time parameterization (each segment gets equal time)
        Point2D getPointAtSegmentTime(float percentage) const {
            if (segments.empty()) return Point2D(0.0f, 0.0f);

            percentage = std::clamp(percentage, 0.0f, 1.0f);

            if (percentage == 0.0f) return segments[0].start;
            if (percentage == 1.0f) return segments.back().end;

            const float segmentCount = static_cast<float>(segments.size());
            const float scaledT = percentage * segmentCount;
            const size_t segmentIndex = static_cast<size_t>(std::floor(scaledT));
            const float segmentT = scaledT - static_cast<float>(segmentIndex);

            // Clamp to valid segment
            const size_t clampedIndex = std::min(segmentIndex, segments.size() - 1);

            return segments[clampedIndex].evaluate(segmentT);
        }

        // Generate lookup tables for X and Y coordinates
        std::pair<std::shared_ptr<juce::dsp::LookupTable<float>>,
                  std::shared_ptr<juce::dsp::LookupTable<float>>>
        generateLookupTables(const bool useArcLength = true, int numPoints = 256) const {

            auto xTable = std::make_shared<juce::dsp::LookupTable<float>>([&](const int n) -> float {
                const float t = static_cast<float>(n) / static_cast<float>(numPoints - 1);
                const Point2D point = useArcLength ? getPointAt(t) : getPointAtSegmentTime(t);
                return point.x;
            }, numPoints);

            auto yTable = std::make_shared<juce::dsp::LookupTable<float>>([&](const int n) -> float {
                const float t = static_cast<float>(n) / static_cast<float>(numPoints - 1);
                const Point2D point = useArcLength ? getPointAt(t) : getPointAtSegmentTime(t);
                return point.y;
            }, numPoints);

            return std::make_pair(xTable, yTable);
        }

        float getTotalLength() const { return totalLength; }
        const std::vector<BezierSegment>& getSegments() const { return segments; }

    private:
        std::vector<BezierSegment> segments;
        std::vector<float> segmentLengths;
        float totalLength = 0.0f;

        void calculateLengths() {
            segmentLengths.clear();
            totalLength = 0.0f;

            for (const auto& segment : segments) {
                const float length = segment.getLength();
                segmentLengths.push_back(length);
                totalLength += length;
            }
        }
    };
}
