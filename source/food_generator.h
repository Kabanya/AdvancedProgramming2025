#pragma once

#include "game_object.h"
#include "math2d.h"
#include "dungeon_generator.h"


class IFoodFabrique : public Component {
public:
    virtual GameObjectPtr create_food(int2 position) = 0;
    virtual int weight() const = 0; // for weighted random selection
};

class FoodGenerator : public Component {
private:
    std::shared_ptr<Dungeon> dungeon;
    std::vector<std::unique_ptr<IFoodFabrique>> fabriques;
    float timeSinceLastSpawn = 0.f; // seconds between spawns
    float spawnInterval = 1.f;
    int fabriquesProbabilitySum = 0;
public:

    FoodGenerator(std::shared_ptr<Dungeon> dungeon, std::vector<std::unique_ptr<IFoodFabrique>> fabriques, float spawnInterval)
        : dungeon(dungeon), fabriques(std::move(fabriques)), spawnInterval(spawnInterval)
    {
        for (const auto& fabrique : this->fabriques)
            fabriquesProbabilitySum += fabrique->weight();
    }

    void generate_random_food()
    {
        auto position = dungeon->getRandomFloorPosition();
        int rand_value = rand() % fabriquesProbabilitySum;
        for (const auto& fabrique : fabriques) {
            if (rand_value < fabrique->weight()) {
                fabrique->create_food(position);
                break;
            }
            rand_value -= fabrique->weight();
        }
    }
    void on_update(float dt) override {
        timeSinceLastSpawn += dt;
        if (timeSinceLastSpawn >= spawnInterval) {
            timeSinceLastSpawn = 0.f;
            // spawn food
            generate_random_food();
        }
    }
};