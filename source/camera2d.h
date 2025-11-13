#pragma once

#include "transform2d.h"
#include <SDL3/SDL_rect.h>

struct Camera2D {
    float pixelsPerMeter;
    Camera2D(float pixelsPerMeter = 32.f)
        : pixelsPerMeter(pixelsPerMeter) {}
};

inline SDL_FRect to_camera_space(const Transform2D& object_transform, const Transform2D& camera_transform, const Camera2D& camera) {
    float camX = (object_transform.x - camera_transform.x) * camera.pixelsPerMeter;
    float camY = (object_transform.y - camera_transform.y) * camera.pixelsPerMeter;
    float camW = object_transform.sizeX * camera.pixelsPerMeter;
    float camH = object_transform.sizeY * camera.pixelsPerMeter;
    return SDL_FRect{camX, camY, camW, camH};
}