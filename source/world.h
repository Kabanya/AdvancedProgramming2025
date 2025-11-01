#pragma once

#include "game_object.h"
#include <memory>
#include <vector>
#include <algorithm>


class World : public std::enable_shared_from_this<World> {
public:
    std::shared_ptr<GameObject> create_object() {
        auto obj = std::make_shared<GameObject>();
        obj->world = shared_from_this();
        delayedAdd.push_back(obj);
        return obj;
    }

    void destroy_object(std::shared_ptr<GameObject> obj) {
        delayedRemove.push_back(obj);
    }

    void update(float dt) {
        for (auto& obj : delayedRemove)
            objects.erase(std::remove(objects.begin(), objects.end(), obj), objects.end());
        delayedRemove.clear();
        for (auto& obj : delayedAdd)
            objects.push_back(obj);
        delayedAdd.clear();

        for (auto& obj : objects) {
            obj->update(dt);
        }
    }

    const std::vector<std::shared_ptr<GameObject>>& get_objects() const {
        return objects;
    }

private:
    std::vector<std::shared_ptr<GameObject>> objects, delayedRemove, delayedAdd;
};
