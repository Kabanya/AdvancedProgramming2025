#include "food_generator.h"
#include "tileset.h"
#include "world.h"
#include "transform2d.h"
#include "health.h"
#include "stamina.h"
#include "background_tag.h"
#include "food.h"


class HealthFood : public IFood {

public:
    int healthRestore;
    HealthFood(int healthRestore) : healthRestore(healthRestore) {}
    void on_consume(GameObjectPtr consumer) override {
        auto health = consumer->get_component<Health>();
        if (health) {
            health->change(healthRestore);
            get_owner()->get_world()->destroy_object(get_owner());
        }
    }
};

class StaminaFood : public IFood {

public:
    int staminaRestore;
    StaminaFood(int staminaRestore) : staminaRestore(staminaRestore) {}
    void on_consume(GameObjectPtr consumer) override {
        auto stamina = consumer->get_component<Stamina>();
        if (stamina) {
            stamina->change(staminaRestore);
            get_owner()->get_world()->destroy_object(get_owner());
        }
    }
};

static GameObjectPtr create_food_abstract(World &world, Sprite sprite, int2 position, IFood *foodComp)
{
    auto foodObj = world.create_object();
    foodObj->add_component<Transform2D>(position.x, position.y);
    foodObj->add_component<Sprite>(sprite); // add appropriate sprite
    foodObj->add_component<IFood>(foodComp); // add appropriate food component
    foodObj->add_component<BackGroundTag>();
    return foodObj;
}

class HealthFoodFabrique : public IFoodFabrique {
    World &world;
    Sprite sprite;
    int healthRestore;
    int weightValue;
public:
    HealthFoodFabrique(World &world, Sprite sprite, int healthRestore, int weightValue)
        : world(world), sprite(sprite), healthRestore(healthRestore), weightValue(weightValue) {}

    virtual GameObjectPtr create_food(int2 position) override
    {
        return create_food_abstract(world, sprite, position, (IFood *)(new HealthFood(healthRestore)));
    }
    virtual int weight() const override { return weightValue; } // for random selection
};


class StaminaFoodFabrique : public IFoodFabrique {
    World &world;
    Sprite sprite;
    int staminaRestore;
    int weightValue;
public:
    StaminaFoodFabrique(World &world, Sprite sprite, int staminaRestore, int weightValue)
        : world(world), sprite(sprite), staminaRestore(staminaRestore), weightValue(weightValue) {}

    virtual GameObjectPtr create_food(int2 position) override
    {
        return create_food_abstract(world, sprite, position, (IFood *)(new StaminaFood(staminaRestore)));
    }
    virtual int weight() const override { return weightValue; } // for random selection
};

std::vector<std::unique_ptr<IFoodFabrique>> create_food_fabriques(World &world, TileSet &tileset)
{
    std::vector<std::unique_ptr<IFoodFabrique>> fabriques;
    fabriques.push_back(std::make_unique<HealthFoodFabrique>(world, tileset.get_tile("health_small"), 10, 100));
    fabriques.push_back(std::make_unique<HealthFoodFabrique>(world, tileset.get_tile("health_large"), 25, 30));

    fabriques.push_back(std::make_unique<StaminaFoodFabrique>(world, tileset.get_tile("stamina_small"), 10, 35));
    fabriques.push_back(std::make_unique<StaminaFoodFabrique>(world, tileset.get_tile("stamina_large"), 25, 20));
    return fabriques;
}