#include "world.h"

#include "image.h"
#include "dungeon_generator.h"
#include "transform2d.h"
#include "sprite.h"
#include "hero.h"
#include "enemy.h"
#include "world.h"
#include "camera2d.h"
#include "dungeon_restrictor.h"
#include "tileset.h"
#include "food_generator.h"
#include "health.h"
#include "stamina.h"
#include "food_consumer.h"
#include "starvation_system.h"
#include "tiredness_system.h"
#include "background_tag.h"
#include "predator.h"
#include <iostream>

const int LevelWidth = 120;
const int LevelHeight = 50;
const int RoomAttempts = 100;
const int BotPopulationCount = 100;
const float PredatorProbability = 0.2f;
const int InitialFoodAmount = 100;

std::vector<std::unique_ptr<IFoodFabrique>> create_food_fabriques(World &world, TileSet &tileset);

void init_world( SDL_Renderer* renderer, World& world)
{

    auto camera = world.create_object();
    camera->add_component<Camera2D>(32.f);
    camera->add_component<Transform2D>(0, 0);


    const int tileSize = 16;
    TexturePtr tilemap = LoadTextureFromFile("assets/kenney_tiny-dungeon/Tilemap/tilemap.png", renderer);
    std::vector<Sprite> sprites;
    if (!tilemap)
    {
        std::cerr << "Failed to load tilemap texture\n";
        return;
    }
    TileSet tileset(tilemap);
    // Возьмем несколько тайлов из тайлсета
    const std::vector<std::pair<int, int>> tileIndices = {
        {4, 0}, // dirty floor
        {4, 1}, // dirty floor
        {3, 4}, // wall
        {8, 1}, // knight
        {9, 0}, // ghost
    };
    for (const auto& [i, j] : tileIndices) {
        sprites.push_back(Sprite(
            tilemap,
            SDL_FRect{float(j * (tileSize + 1)), float(i * (tileSize + 1)), float(tileSize), float(tileSize)}
        ));
    }

    auto dungeon = std::make_shared<Dungeon>(LevelWidth, LevelHeight, RoomAttempts);
    const auto &grid = dungeon->getGrid();
    for (int i = 0; i < LevelHeight; ++i)
        for (int j = 0; j < LevelWidth; ++j)
        {
            const char * spriteName = nullptr;
            if (grid[i][j] == Dungeon::FLOOR) {
                spriteName = rand() % 2 == 0 ? "floor1" : "floor2";
            } else if (grid[i][j] == Dungeon::WALL) {
                spriteName = "wall";
            }
            if (spriteName) {
                auto newCell = world.create_object();
                newCell->add_component<Sprite>(tileset.get_tile(spriteName));
                newCell->add_component<Transform2D>(j, i);
                newCell->add_component<BackGroundTag>();
            }
        }

    auto heroPos = dungeon->getRandomFloorPosition();
    auto hero = world.create_object();
    hero->add_component<Sprite>(tileset.get_tile("knight"));
    hero->add_component<Transform2D>(heroPos.x, heroPos.y);
    hero->add_component<Hero>(camera);
    hero->add_component<IRestrictor>((IRestrictor *)(new DungeonRestrictor(dungeon)));
    hero->add_component<Health>(100);
    hero->add_component<Stamina>(100);
    hero->add_component<FoodConsumer>();

    for (int e = 0; e < BotPopulationCount; ++e) {
        const bool isPredator = (rand() % 100) < int(PredatorProbability * 100.f);
        auto enemyPos = dungeon->getRandomFloorPosition();
        auto enemy = world.create_object();
        enemy->add_component<Sprite>(isPredator ? tileset.get_tile("ghost") : tileset.get_tile("peasant"));
        enemy->add_component<Transform2D>(enemyPos.x, enemyPos.y);
        enemy->add_component<Enemy>();
        enemy->add_component<IRestrictor>((IRestrictor *)(new DungeonRestrictor(dungeon)));
        enemy->add_component<Health>(100);
        enemy->add_component<Stamina>(100);
        if (isPredator)
             enemy->add_component<Predator>();
        else
            enemy->add_component<FoodConsumer>();
    }

    auto foodFabriques = create_food_fabriques(world, tileset);

    auto foodGenerator = world.create_object();
    auto generatorComp = foodGenerator->add_component<FoodGenerator>(dungeon, std::move(foodFabriques), 2.f / RoomAttempts);
    for (int i = 0; i < InitialFoodAmount; i++)
        generatorComp->generate_random_food();
    auto starvation = world.create_object();
    starvation->add_component<StarvationSystem>();
    auto tiredness = world.create_object();
    tiredness->add_component<TirednessSystem>();
}