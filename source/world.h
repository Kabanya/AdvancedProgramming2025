#pragma once

#include <memory>
#include <vector>
#include <variant>
#include <mutex>
// #include "thread_pool.h"

#include "sprite.h"
#include "transform2d.h"
#include "health.h"
#include "stamina.h"
#include "camera2d.h"
#include "fsm.h"
#include "math2d.h"

extern std::mutex g_worldMutex;
// extern ThreadPool g_threadPool(4);

// Forward declarations
class World;
class Dungeon;
class DungeonRestrictor;

// Food variant types
struct HealthFood {
    int restore;
};

struct StaminaFood {
    int restore;
};

using FoodVariant = std::variant<HealthFood, StaminaFood>;

// Hero behavior data
struct HeroData {
    float timeSinceLastMove = 0.f;
    size_t cameraIndex = 0; // index into camera archetype
};

// NPC/Enemy behavior data
struct NPCData {
    float accumulatedTime = 0.f;
    NPCState state;
    int2 targetPos = {-1, -1}; // Current target position for pathfinding
};

// NPC type variant
struct NPCConsumer {}; // NPC that consumes food
struct NPCPredator {}; // NPC that hunts other NPCs

using NPCType = std::variant<NPCConsumer, NPCPredator>;

// SaO
struct TilesArchetype {
    std::vector<Sprite> sprite;
    std::vector<Transform2D> transform;
    // Deferred operations
    std::vector<size_t> delayedRemove;

    size_t size() const { return sprite.size(); }
};

struct HeroArchetype {
    std::vector<Sprite> sprite;
    std::vector<Transform2D> transform;
    std::vector<Health> health;
    std::vector<Stamina> stamina;
    std::vector<std::shared_ptr<DungeonRestrictor>> restrictor;
    std::vector<HeroData> heroData;

    // Deferred operations
    std::vector<size_t> delayedRemove;

    size_t size() const { return sprite.size(); }
};

struct NPCArchetype {
    std::vector<Sprite> sprite;
    std::vector<Transform2D> transform;
    std::vector<Health> health;
    std::vector<Stamina> stamina;
    std::vector<std::shared_ptr<DungeonRestrictor>> restrictor;
    std::vector<NPCData> npcData;
    std::vector<NPCType> npcType;

    // Deferred operations
    std::vector<size_t> delayedRemove;

    size_t size() const { return sprite.size(); }
};

struct FoodArchetype {
    std::vector<Sprite> sprite;
    std::vector<Transform2D> transform;
    std::vector<FoodVariant> foodType;

    // Deferred operations
    std::vector<size_t> delayedRemove;

    size_t size() const { return sprite.size(); }
};

struct CameraArchetype {
    std::vector<Transform2D> transform;
    std::vector<Camera2D> camera;

    size_t size() const { return transform.size(); }
};

// Systems
struct StarvationSystemData {
    float accumulator = 0.0f;
    const float damageInterval = 1.0f;
    const int damageAmount = 2;
};

struct TirednessSystemData {
    float accumulator = 0.0f;
    const float tirednessInterval = 1.0f;
    const int tirednessAmount = 5;
};

struct FoodGeneratorData {
    std::shared_ptr<Dungeon> dungeon;
    float timeSinceLastSpawn = 0.f;
    float spawnInterval = 1.f;

    // Food generation weights and data
    struct FoodTypeInfo {
        FoodVariant foodType;
        Sprite sprite;
        int weight;
    };
    std::vector<FoodTypeInfo> foodTypes;
    int totalWeight = 0;
};

// Helper macros for archetype operations
#define ARCHETYPE_ADD(archetype, ...) \
    archetype.sprite.push_back(__VA_ARGS__)

#define ARCHETYPE_REMOVE_DEFERRED(archetype, index) \
    archetype.delayedRemove.push_back(index)

#define ARCHETYPE_PROCESS_REMOVALS(archetype) \
    do { \
        if (!archetype.delayedRemove.empty()) { \
            std::sort(archetype.delayedRemove.begin(), archetype.delayedRemove.end(), std::greater<size_t>()); \
            for (size_t idx : archetype.delayedRemove) { \
                if (idx < archetype.sprite.size()) { \
                    archetype.sprite.erase(archetype.sprite.begin() + idx); \
                    archetype.transform.erase(archetype.transform.begin() + idx); \
                    if constexpr (requires { archetype.health; }) { \
                        archetype.health.erase(archetype.health.begin() + idx); \
                    } \
                    if constexpr (requires { archetype.stamina; }) { \
                        archetype.stamina.erase(archetype.stamina.begin() + idx); \
                    } \
                    if constexpr (requires { archetype.restrictor; }) { \
                        archetype.restrictor.erase(archetype.restrictor.begin() + idx); \
                    } \
                    if constexpr (requires { archetype.heroData; }) { \
                        archetype.heroData.erase(archetype.heroData.begin() + idx); \
                    } \
                    if constexpr (requires { archetype.npcData; }) { \
                        archetype.npcData.erase(archetype.npcData.begin() + idx); \
                    } \
                    if constexpr (requires { archetype.npcType; }) { \
                        archetype.npcType.erase(archetype.npcType.begin() + idx); \
                    } \
                    if constexpr (requires { archetype.foodType; }) { \
                        archetype.foodType.erase(archetype.foodType.begin() + idx); \
                    } \
                } \
            } \
            archetype.delayedRemove.clear(); \
        } \
    } while(0)

class World {
public:
    // Archetypes
    TilesArchetype tiles;
    HeroArchetype hero;
    NPCArchetype npcs;
    FoodArchetype food;
    CameraArchetype camera;

    // Systems
    StarvationSystemData starvationSystem;
    TirednessSystemData tirednessSystem;
    FoodGeneratorData foodGenerator;

    void update(float dt);
    void update_ts(float dt);
    void world_update_threaded(float dt);
    void world_update_thread_pool(float dt);

    // Helper methods for adding entities
    void add_tile(const Sprite& sprite, const Transform2D& transform);
    void add_hero(const Sprite& sprite, const Transform2D& transform,
                  std::shared_ptr<DungeonRestrictor> restrictor, size_t cameraIndex);
    void add_npc(const Sprite& sprite, const Transform2D& transform,
                 std::shared_ptr<DungeonRestrictor> restrictor, NPCType type);
    void add_food(const Sprite& sprite, const Transform2D& transform, FoodVariant foodType);
    size_t add_camera(const Transform2D& transform, const Camera2D& cam);

    // Helper methods for removing entities (deferred)
    void remove_npc(size_t index);
    void remove_food(size_t index);

private: // Base
    void process_deferred_removals();
    void update_hero(float dt);
    void update_npcs(float dt);
    void update_food_consumption(float dt);
    void update_predators(float dt);
    void update_reproduction(float dt);
    void update_starvation_system(float dt);
    void update_tiredness_system(float dt);
    void update_food_generator(float dt);

private: // Thread-safe
    void process_deferred_removals_ts();
    void update_hero_ts(float dt);
    void update_npcs_ts(float dt);
    void update_food_consumption_ts(float dt);
    void update_predators_ts(float dt);
    void update_reproduction_ts(float dt);
    void update_starvation_system_ts(float dt);
    void update_tiredness_system_ts(float dt);
    void update_food_generator_ts(float dt);


private: // Thread-pool
    void process_deferred_removals_tp();
    void update_hero_tp(float dt);
    void update_npcs_tp(float dt);
    void update_food_consumption_tp(float dt);
    void update_predators_tp(float dt);
    void update_reproduction_tp(float dt);
    void update_starvation_system_tp(float dt);
    void update_tiredness_system_tp(float dt);
    void update_food_generator_tp(float dt);
};

// Inline implementations
inline void World::add_tile(const Sprite& sprite, const Transform2D& transform) {
    tiles.sprite.push_back(sprite);
    tiles.transform.push_back(transform);
}

inline void World::add_hero(const Sprite& sprite, const Transform2D& transform,
                            std::shared_ptr<DungeonRestrictor> restrictor, size_t cameraIndex) {
    hero.sprite.push_back(sprite);
    hero.transform.push_back(transform);
    hero.health.push_back(Health(100));
    hero.stamina.push_back(Stamina(100));
    hero.restrictor.push_back(restrictor);
    hero.heroData.push_back(HeroData{0.f, cameraIndex});
}

inline void World::add_npc(const Sprite& sprite, const Transform2D& transform,
                           std::shared_ptr<DungeonRestrictor> restrictor, NPCType type) {
    npcs.sprite.push_back(sprite);
    npcs.transform.push_back(transform);
    npcs.health.push_back(Health(100));
    npcs.stamina.push_back(Stamina(100));
    npcs.restrictor.push_back(restrictor);

    // Initialize NPCData with appropriate state based on type
    NPCData data;
    data.accumulatedTime = 0.f;
    data.targetPos = {-1, -1};
    data.state = std::holds_alternative<NPCConsumer>(type)
        ? NPCState(ConsumerState::IDLE)
        : NPCState(PredatorState::IDLE);
    npcs.npcData.push_back(data);
    npcs.npcType.push_back(type);
}

inline void World::add_food(const Sprite& sprite, const Transform2D& transform, FoodVariant foodType) {
    food.sprite.push_back(sprite);
    food.transform.push_back(transform);
    food.foodType.push_back(foodType);
}

inline size_t World::add_camera(const Transform2D& transform, const Camera2D& cam) {
    camera.transform.push_back(transform);
    camera.camera.push_back(cam);
    return camera.size() - 1;
}

inline void World::remove_npc(size_t index) {
    if (index < npcs.size()) {
        npcs.delayedRemove.push_back(index);
    }
}

inline void World::remove_food(size_t index) {
    if (index < food.size()) {
        food.delayedRemove.push_back(index);
    }
}