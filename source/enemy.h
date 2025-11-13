#pragma once
#include "game_object.h"
#include "restrictor.h"
#include "stamina.h"
#include "transform2d.h"
#include <SDL3/SDL.h>

class Enemy : public Component {
private:
    // change transform by 1.0 unit when accumulatedDelta reaches 1.0
    float accumulatedTime;

public:
    void on_update(float dt) override {
        auto transform = get_owner()->get_component<Transform2D>();
        auto restrictor = get_owner()->get_component<IRestrictor>();
        auto stamina = get_owner()->get_component<Stamina>();
        if (!transform || !restrictor || !stamina)
            return;
        accumulatedTime += dt * stamina->get_speed();
        if (accumulatedTime < 1.0f)
            return;
        accumulatedTime -= 1.0f;
        const int2 directions [] = { int2{1,0}, int2{-1,0}, int2{0,1}, int2{0,-1} };
        // try to move in a random direction
        int i = rand() % 4;
        int2 intDelta = directions[i];
        int2 newPos = int2((int)transform->x + intDelta.x, (int)transform->y + intDelta.y);
        if (restrictor->can_pass(newPos)) {
            transform->x += intDelta.x;
            transform->y += intDelta.y;
        }
    }
};
