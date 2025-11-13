#pragma once

#include "component.h"
#include "food.h"
#include "game_object.h"
#include "transform2d.h"
#include "world.h"

class FoodConsumer : public Component {
public:
    void on_update(float dt) override {
        auto myTransform = get_owner()->get_component<Transform2D>();
        for (auto& obj : get_owner()->get_world()->get_objects()) {
            auto food = obj->get_component<IFood>();
            if (food) {
                // Simple collision check (assuming both have Transform2D)
                auto foodTransform = obj->get_component<Transform2D>();
                if (myTransform && foodTransform) {
                    if (int(myTransform->x) == int(foodTransform->x) &&
                        int(myTransform->y) == int(foodTransform->y)) {
                        food->on_consume(get_owner());
                        break; // Consume only one food at a time
                    }
                }
            }
        }
    }
};