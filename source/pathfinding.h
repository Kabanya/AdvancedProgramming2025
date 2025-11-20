#pragma once

#include "math2d.h"
#include <vector>
#include <functional>

struct Int2Hash {
    size_t operator()(const int2& p) const {
        size_t h1 = std::hash<int>{}(p.x);
        size_t h2 = std::hash<int>{}(p.y);
        return h1 ^ (h2 << 1);
    }
};

struct AStarNode {
    int2 pos;
    float g;
    float h;

    float f() const { return g + h; }

    bool operator>(const AStarNode& other) const {
        return f() > other.f();
    }
};

std::vector<int2> astar(const int2& start,
                        const int2& goal,
                        std::function<bool(const int2&)> can_pass,
                        std::function<bool(const int2&)> filter = nullptr);
