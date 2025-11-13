#pragma once
#include "game_object.h"
#include "restrictor.h"
#include "stamina.h"
#include <SDL3/SDL.h>
#include "transform2d.h"

class Hero : public Component {
private:
    float timeSinceLastMode = 0.f; // seconds between movement steps
    GameObjectPtr mainCamera;

    void bind_camera_transform() {
        auto transform = get_owner()->get_component<Transform2D>();

        if (mainCamera && transform) {
            auto camTransform = mainCamera->get_component<Transform2D>();
            if (camTransform) {
                camTransform->x = transform->x;
                camTransform->y = transform->y;
            }
        }
    }

public:
    Hero(GameObjectPtr mainCamera = nullptr)
        : mainCamera(mainCamera) {}

    void on_create() override {
        bind_camera_transform();
    }

    void on_update(float dt) override {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto transform = get_owner()->get_component<Transform2D>();
        auto restrictor = get_owner()->get_component<IRestrictor>();
        auto stamina = get_owner()->get_component<Stamina>();
        if (!transform || !restrictor || !stamina)
            return;
        const float cellPerSecond = stamina->get_speed();
        int2 intDelta;
        bool moved = false;
        if (keys[SDL_SCANCODE_W]) { intDelta.y -= 1; moved = true; }
        if (keys[SDL_SCANCODE_S]) { intDelta.y += 1; moved = true; }
        if (keys[SDL_SCANCODE_A]) { intDelta.x -= 1; moved = true; }
        if (keys[SDL_SCANCODE_D]) { intDelta.x += 1; moved = true; }
        if (!moved)
            return;
        if (moved && (timeSinceLastMode < 1.f / cellPerSecond)) {
            timeSinceLastMode += dt;
            return;
        }
        timeSinceLastMode = 0.f;
        int2 newPos = int2((int)transform->x + intDelta.x, (int)transform->y + intDelta.y);
        if (restrictor->can_pass(newPos)) {
            transform->x += intDelta.x;
            transform->y += intDelta.y;
            bind_camera_transform();
        }
    }
};
