#pragma once

namespace imagiro {
    struct Point2D {
        float x, y;

        explicit Point2D(const float x = 0.f, const float y = 0.f) : x(x), y(y) {}

        Point2D operator+(const Point2D& other) const {
            return Point2D(x + other.x, y + other.y);
        }

        Point2D operator-(const Point2D& other) const {
            return Point2D(x - other.x, y - other.y);
        }

        Point2D operator*(const float scalar) const {
            return Point2D(x * scalar, y * scalar);
        }
    };
}