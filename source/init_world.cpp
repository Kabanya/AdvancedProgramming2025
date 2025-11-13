#include "world.h"
#include "image.h"
#include "dungeon_generator.h"
#include "dungeon_restrictor.h"
#include "transform2d.h"
#include "tileset.h"
#include <iostream>
#include <string>

const int LevelWidth = 120;
const int LevelHeight = 50;
const int RoomAttempts = 100;
const int BotPopulationCount = 100;
const float PredatorProbability = 0.2f;
const int InitialFoodAmount = 100;

void init_world(SDL_Renderer* renderer, World& world)
{
    const int tileSize = 16;
    TexturePtr tilemap = LoadTextureFromFile("assets/kenney_tiny-dungeon/Tilemap/tilemap.png", renderer);
    if (!tilemap) {
        std::cerr << "Failed to load tilemap texture\n";
        return;
    }
    TileSet tileset(tilemap);
    // Create camera first
    size_t cameraIndex = world.add_camera(Transform2D(0, 0), Camera2D(32.f));
    // Generate dungeon
    auto dungeon = std::make_shared<Dungeon>(LevelWidth, LevelHeight, RoomAttempts);
    const auto& grid = dungeon->getGrid();
    // Create background tiles
    world.tiles.sprite.reserve(LevelWidth * LevelHeight);
    world.tiles.transform.reserve(LevelWidth * LevelHeight);
    for (int i = 0; i < LevelHeight; ++i) {
        for (int j = 0; j < LevelWidth; ++j) {
            std::string spriteName;
            if (grid[i][j] == Dungeon::FLOOR) {
                spriteName = rand() % 2 == 0 ? "floor1" : "floor2";
            } else if (grid[i][j] == Dungeon::WALL) {
                spriteName = "wall";
            }
            if (!spriteName.empty()) {
                world.add_tile(tileset.get_tile(spriteName), Transform2D(j, i));
            }
        }
    }
    // Create hero
    auto heroPos = dungeon->getRandomFloorPosition();
    auto heroRestrictor = std::make_shared<DungeonRestrictor>(dungeon);
    world.add_hero(
        tileset.get_tile(std::string("knight")),
        Transform2D(heroPos.x, heroPos.y),
        heroRestrictor,
        cameraIndex
    );
    // Set initial camera position to hero
    if (cameraIndex < world.camera.size()) {
        world.camera.transform[cameraIndex].x = heroPos.x;
        world.camera.transform[cameraIndex].y = heroPos.y;
    }
    // Create NPCs (enemies)
    world.npcs.sprite.reserve(BotPopulationCount);
    world.npcs.transform.reserve(BotPopulationCount);
    world.npcs.health.reserve(BotPopulationCount);
    world.npcs.stamina.reserve(BotPopulationCount);
    world.npcs.restrictor.reserve(BotPopulationCount);
    world.npcs.npcData.reserve(BotPopulationCount);
    world.npcs.npcType.reserve(BotPopulationCount);
    for (int e = 0; e < BotPopulationCount; ++e) {
        const bool isPredator = (rand() % 100) < int(PredatorProbability * 100.f);
        auto enemyPos = dungeon->getRandomFloorPosition();
        auto enemyRestrictor = std::make_shared<DungeonRestrictor>(dungeon);

        NPCType npcType = isPredator ? NPCType(NPCPredator{}) : NPCType(NPCConsumer{});

        world.add_npc(
            isPredator ? tileset.get_tile(std::string("ghost")) : tileset.get_tile(std::string("peasant")),
            Transform2D(enemyPos.x, enemyPos.y),
            enemyRestrictor,
            npcType
        );
    }

    // Initialize food generator
    world.foodGenerator.dungeon = dungeon;
    world.foodGenerator.spawnInterval = 2.f / RoomAttempts;
    world.foodGenerator.totalWeight = 0;

    // Setup food types with weights
    world.foodGenerator.foodTypes.push_back({
        FoodVariant(HealthFood{10}),
        tileset.get_tile(std::string("health_small")),
        100
    });
    world.foodGenerator.foodTypes.push_back({
        FoodVariant(HealthFood{25}),
        tileset.get_tile(std::string("health_large")),
        30
    });
    world.foodGenerator.foodTypes.push_back({
        FoodVariant(StaminaFood{10}),
        tileset.get_tile(std::string("stamina_small")),
        35
    });
    world.foodGenerator.foodTypes.push_back({
        FoodVariant(StaminaFood{25}),
        tileset.get_tile(std::string("stamina_large")),
        20
    });

    for (const auto& foodType : world.foodGenerator.foodTypes) {
        world.foodGenerator.totalWeight += foodType.weight;
    }

    // Generate initial food
    world.food.sprite.reserve(InitialFoodAmount);
    world.food.transform.reserve(InitialFoodAmount);
    world.food.foodType.reserve(InitialFoodAmount);

    for (size_t i = 0; i < InitialFoodAmount; i++) {
        auto position = dungeon->getRandomFloorPosition();
        int randValue = rand() % world.foodGenerator.totalWeight;
        for (const auto& foodTypeInfo : world.foodGenerator.foodTypes) {
            if (randValue < foodTypeInfo.weight) {
                world.add_food(foodTypeInfo.sprite, Transform2D(position.x, position.y), foodTypeInfo.foodType);
                break;
            }
            randValue -= foodTypeInfo.weight;
        }
    }
}