#pragma once

struct int2 {
    int x, y;
    int2(int x=0, int y=0) : x(x), y(y) {}
};

inline bool operator==(const int2& a, const int2& b)
{
    return a.x == b.x && a.y == b.y;
}

struct float2 {
    float x, y;
    float2(float x=0, float y=0) : x(x), y(y) {}
};