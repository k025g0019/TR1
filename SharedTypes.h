#pragma once

#include <algorithm>
#include <cstdint>

constexpr int kGridWidth = 10;
constexpr int kGridHeight = 10;
constexpr int kActionCount = 4;
constexpr unsigned int kWindowWidth = 1400;
constexpr unsigned int kWindowHeight = 920;
constexpr int kTrainingStepsPerFrame = 1;

struct GridPoint {
    int x = 0;
    int y = 0;
};

enum class Tile {
    Empty,
    Start,
    Goal,
    Wall,
    Pit,
};

enum class Action : int {
    Up = 0,
    Right = 1,
    Down = 2,
    Left = 3,
};

struct Color {
    float r;
    float g;
    float b;
    float a;
};

struct Vertex {
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float a;
};

inline float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

inline float NormalizeRange(float value, float minValue, float maxValue) {
    if (maxValue <= minValue) {
        return 0.0f;
    }
    return Clamp01((value - minValue) / (maxValue - minValue));
}

inline Color LerpColor(const Color& from, const Color& to, float t) {
    const float clamped = Clamp01(t);
    return {
        from.r + (to.r - from.r) * clamped,
        from.g + (to.g - from.g) * clamped,
        from.b + (to.b - from.b) * clamped,
        from.a + (to.a - from.a) * clamped,
    };
}
