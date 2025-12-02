#include <SDL3/SDL.h>
#include <algorithm>
#include <mutex>

#include "world.h"
#include "dungeon_generator.h"  // IWYU pragma: keep
#include "dungeon_restrictor.h" // IWYU pragma: keep

#include "config.h" // here definition of macros

std::mutex g_worldMutex;

// ----------------------- AI TYPES ------------------------
#if USE_BEHAVIOUR_TREE
    #include "bt.h"
    // Forward declarations for BT
    BTStatus update_consumer_bt(size_t npc_index, World& world, const int2 directions[4]);
    BTStatus update_predator_bt(size_t npc_index, World& world, const int2 directions[4]);
#else
    #include "fsm.h"
    // Forward declarations for FSM
    void update_consumer_fsm(size_t npc_index, World& world, const int2 directions[4]);
    void update_predator_fsm(size_t npc_index, World& world, const int2 directions[4]);
#endif


// ----------------------- WORLD UPDATE TYPES ------------------------

#if USE_THREAD_POOL && !USE_MUTEX && !USE_THREADS // THREAD POOL WORLD UPDATE
    #include "thread_pool.h"   // IWYU pragma: keep
    ThreadPool g_threadPool(4);
    void world_update_thread_pool(World& world, float dt)
    {
        g_threadPool.Start();
        // g_threadPool.Submit([&]() {
        //     hero_input_system(world, dt);
        // });
        // ...
        // g_threadPool.wait_all_task_done();
    }
#endif // USE_THREAD_POOL

#if USE_THREADS && !USE_MUTEX && !USE_THREAD_POOL // THREADS WORLD UPDATE
    #include <thread>
    void World::world_update_threaded(float dt) {
        std::thread heroThread(&World::update_hero, this, dt);
        std::thread npcsThread(&World::update_npcs, this, dt);
        std::thread foodThread(&World::update_food_consumption, this, dt);
        std::thread predatorsThread(&World::update_predators, this, dt);
        std::thread reproductionThread(&World::update_reproduction, this, dt);
        std::thread starvationThread(&World::update_starvation_system, this, dt);
        std::thread tirednessThread(&World::update_tiredness_system, this, dt);
        std::thread foodGeneratorThread(&World::update_food_generator, this, dt);
        // std::thread removalsThread(&World::process_deferred_removals, this);
        heroThread.join();
        npcsThread.join();
        foodThread.join();
        predatorsThread.join();
        reproductionThread.join();
        starvationThread.join();
        tirednessThread.join();
        foodGeneratorThread.join();
        // removalsThread.join();
        World::process_deferred_removals();
    }
#endif // USE_THREADS

#if USE_MUTEX && !USE_THREADS && !USE_THREAD_POOL // MUTEX WORLD UPDATE
    void World::update_ts(float dt) {
        update_hero_ts(dt);
        update_npcs_ts(dt);
        update_food_consumption_ts(dt);
        update_predators_ts(dt);
        update_reproduction_ts(dt);
        update_starvation_system_ts(dt);
        update_tiredness_system_ts(dt);
        update_food_generator_ts(dt);
        process_deferred_removals_ts();
    }
#endif

#if !USE_THREADS && !USE_THREAD_POOL && !USE_MUTEX // STANDARD WORLD UPDATE
    void World::update(float dt) {
        update_hero(dt);
        update_npcs(dt);
        update_food_consumption(dt);
        update_predators(dt);
        update_reproduction(dt);
        update_starvation_system(dt);
        update_tiredness_system(dt);
        update_food_generator(dt);
        process_deferred_removals();
    }
#endif // USE_MUTEX // FOR_WORLD_UPDATE


void World::process_deferred_removals() {
    // Process hero removals
    if (!hero.delayedRemove.empty()) {
        std::sort(hero.delayedRemove.begin(), hero.delayedRemove.end(), std::greater<size_t>());
        for (size_t idx : hero.delayedRemove) {
            if (idx < hero.size()) {
                hero.sprite.erase(hero.sprite.begin() + idx);
                hero.transform.erase(hero.transform.begin() + idx);
                hero.health.erase(hero.health.begin() + idx);
                hero.stamina.erase(hero.stamina.begin() + idx);
                hero.restrictor.erase(hero.restrictor.begin() + idx);
                hero.heroData.erase(hero.heroData.begin() + idx);
            }
        }
        hero.delayedRemove.clear();
    }

    // Process NPC removals
    if (!npcs.delayedRemove.empty()) {
        std::sort(npcs.delayedRemove.begin(), npcs.delayedRemove.end(), std::greater<size_t>());
        for (size_t idx : npcs.delayedRemove) {
            if (idx < npcs.size()) {
                npcs.sprite.erase(npcs.sprite.begin() + idx);
                npcs.transform.erase(npcs.transform.begin() + idx);
                npcs.health.erase(npcs.health.begin() + idx);
                npcs.stamina.erase(npcs.stamina.begin() + idx);
                npcs.restrictor.erase(npcs.restrictor.begin() + idx);
                npcs.npcData.erase(npcs.npcData.begin() + idx);
                npcs.npcType.erase(npcs.npcType.begin() + idx);
            }
        }
        npcs.delayedRemove.clear();
    }

    // Process food removals
    if (!food.delayedRemove.empty()) {
        std::sort(food.delayedRemove.begin(), food.delayedRemove.end(), std::greater<size_t>());
        for (size_t idx : food.delayedRemove) {
            if (idx < food.size()) {
                food.sprite.erase(food.sprite.begin() + idx);
                food.transform.erase(food.transform.begin() + idx);
                food.foodType.erase(food.foodType.begin() + idx);
            }
        }
        food.delayedRemove.clear();
    }
}

void World::update_hero(float dt) {
    if (hero.size() == 0) return;

    const bool* keys = SDL_GetKeyboardState(nullptr);

    for (size_t i = 0; i < hero.size(); ++i) {
        auto& transform = hero.transform[i];
        auto& restrictor = hero.restrictor[i];
        auto& stamina = hero.stamina[i];
        auto& heroData = hero.heroData[i];

        const float cellPerSecond = stamina.get_speed();
        int2 intDelta{0, 0};
        bool moved = false;

        if (keys[SDL_SCANCODE_W]) { intDelta.y -= 1; moved = true; }
        if (keys[SDL_SCANCODE_S]) { intDelta.y += 1; moved = true; }
        if (keys[SDL_SCANCODE_A]) { intDelta.x -= 1; moved = true; }
        if (keys[SDL_SCANCODE_D]) { intDelta.x += 1; moved = true; }

        if (!moved)
            continue;

        if (moved && (heroData.timeSinceLastMove < 1.f / cellPerSecond)) {
            heroData.timeSinceLastMove += dt;
            continue;
        }

        heroData.timeSinceLastMove = 0.f;
        int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);

        if (restrictor->can_pass(newPos)) {
            transform.x += intDelta.x;
            transform.y += intDelta.y;
            // Bind camera to hero position
            if (heroData.cameraIndex < camera.size()) {
                camera.transform[heroData.cameraIndex].x = transform.x;
                camera.transform[heroData.cameraIndex].y = transform.y;
            }
        }
    }
}

void World::update_npcs(float dt) {
    const int2 directions[] = { int2{1,0}, int2{-1,0}, int2{0,1}, int2{0,-1} };

    for (size_t i = 0; i < npcs.size(); ++i) {
        auto& stamina = npcs.stamina[i];
        auto& npcData = npcs.npcData[i];

        npcData.accumulatedTime += dt * stamina.get_speed();
        if (npcData.accumulatedTime < 1.0f)
            continue;

        npcData.accumulatedTime -= 1.0f;

#if USE_BEHAVIOUR_TREE
        // Update Behaviour Tree based on NPC type
        if (std::holds_alternative<NPCConsumer>(npcs.npcType[i])) {
            update_consumer_bt(i, *this, directions);
        } else if (std::holds_alternative<NPCPredator>(npcs.npcType[i])) {
            update_predator_bt(i, *this, directions);
        }
#else
        // Update FSM based on NPC type
        if (std::holds_alternative<NPCConsumer>(npcs.npcType[i])) {
            update_consumer_fsm(i, *this, directions);
        } else if (std::holds_alternative<NPCPredator>(npcs.npcType[i])) {
            update_predator_fsm(i, *this, directions);
        }
#endif
    }
}

void World::update_food_consumption(float dt) {
    // Hero consumes food
    for (size_t h = 0; h < hero.size(); ++h) {
        bool heroMarkedForRemoval = false;
        for (size_t idx : hero.delayedRemove) {
            if (idx == h) {
                heroMarkedForRemoval = true;
                break;
            }
        }
        if (heroMarkedForRemoval)
            continue;

        auto& heroTransform = hero.transform[h];
        auto& heroHealth = hero.health[h];
        auto& heroStamina = hero.stamina[h];

        for (size_t f = 0; f < food.size(); ++f) {
            bool foodMarkedForRemoval = false;
            for (size_t idx : food.delayedRemove) {
                if (idx == f) {
                    foodMarkedForRemoval = true;
                    break;
                }
            }
            if (foodMarkedForRemoval)
                continue;

            auto& foodTransform = food.transform[f];

            // Check collision
            if (int(heroTransform.x) == int(foodTransform.x) &&
                int(heroTransform.y) == int(foodTransform.y)) {
                // Apply food effect
                std::visit([&](auto&& foodType) {
                    using T = std::decay_t<decltype(foodType)>;
                    if constexpr (std::is_same_v<T, HealthFood>) {
                        heroHealth.change(foodType.restore);
                    } else if constexpr (std::is_same_v<T, StaminaFood>) {
                        heroStamina.change(foodType.restore);
                    }
                }, food.foodType[f]);

                remove_food(f);
                break;
            }
        }
    }

    // NPCs consume food
    for (size_t n = 0; n < npcs.size(); ++n) {
        // Skip NPC if already marked for removal
        bool npcMarkedForRemoval = false;
        for (size_t idx : npcs.delayedRemove) {
            if (idx == n) {
                npcMarkedForRemoval = true;
                break;
            }
        }
        if (npcMarkedForRemoval)
            continue;

        auto& npcTransform = npcs.transform[n];
        auto& npcHealth = npcs.health[n];
        auto& npcStamina = npcs.stamina[n];

        for (size_t f = 0; f < food.size(); ++f) {
            // Skip food if already marked for removal
            bool foodMarkedForRemoval = false;
            for (size_t idx : food.delayedRemove) {
                if (idx == f) {
                    foodMarkedForRemoval = true;
                    break;
                }
            }
            if (foodMarkedForRemoval)
                continue;

            auto& foodTransform = food.transform[f];

            // Check collision
            if (int(npcTransform.x) == int(foodTransform.x) &&
                int(npcTransform.y) == int(foodTransform.y)) {
                // Apply food effect
                std::visit([&](auto&& foodType) {
                    using T = std::decay_t<decltype(foodType)>;
                    if constexpr (std::is_same_v<T, HealthFood>) {
                        npcHealth.change(foodType.restore);
                    } else if constexpr (std::is_same_v<T, StaminaFood>) {
                        npcStamina.change(foodType.restore);
                    }
                }, food.foodType[f]);

                remove_food(f);
                break;
            }
        }
    }
}

void World::update_predators(float dt) {
    // Predators hunt consumer NPCs and hero
    for (size_t p = 0; p < npcs.size(); ++p) {
        // Check if this NPC is a predator
        if (!std::holds_alternative<NPCPredator>(npcs.npcType[p]))
            continue;

        // Check if this predator was already marked for removal
        bool predatorMarkedForRemoval = false;
        for (size_t idx : npcs.delayedRemove) {
            if (idx == p) {
                predatorMarkedForRemoval = true;
                break;
            }
        }
        if (predatorMarkedForRemoval)
            continue;
        auto& predatorTransform = npcs.transform[p];
        auto& predatorHealth = npcs.health[p];
        bool victimFound = false;
        // First, check if hero can be hunted
        for (size_t h = 0; h < hero.size(); ++h) {
            // Check if hero is already marked for removal
            bool heroMarkedForRemoval = false;
            for (size_t idx : hero.delayedRemove) {
                if (idx == h) {
                    heroMarkedForRemoval = true;
                    break;
                }
            }
            if (heroMarkedForRemoval)
                continue;
            auto& heroTransform = hero.transform[h];
            auto& heroHealth = hero.health[h];
            // Check collision
            if (int(predatorTransform.x) == int(heroTransform.x) &&
                int(predatorTransform.y) == int(heroTransform.y)) {
                // Heal predator by hero's current health
                predatorHealth.change(heroHealth.current);
                // Kill hero
                hero.delayedRemove.push_back(h);
                victimFound = true;
                break;
            }
        }
        if (victimFound)
            continue; // Predator already ate, skip to next predator
        // Look for victims (consumer NPCs)
        for (size_t v = 0; v < npcs.size(); ++v) {
            if (p == v) continue; // Skip self
            // Check if victim is already marked for removal
            bool victimMarkedForRemoval = false;
            for (size_t idx : npcs.delayedRemove) {
                if (idx == v) {
                    victimMarkedForRemoval = true;
                    break;
                }
            }
            if (victimMarkedForRemoval)
                continue;
            // Check if victim is a consumer
            if (!std::holds_alternative<NPCConsumer>(npcs.npcType[v]))
                continue;
            auto& victimTransform = npcs.transform[v];
            auto& victimHealth = npcs.health[v];
            // Check collision
            if (int(predatorTransform.x) == int(victimTransform.x) &&
                int(predatorTransform.y) == int(victimTransform.y)) {
                // Heal predator by victim's current health
                predatorHealth.change(victimHealth.current);
                // Kill victim
                remove_npc(v);
                break; // Consume only one victim at a time
            }
        }
    }
}

void World::update_starvation_system(float dt) {
    starvationSystem.accumulator += dt;
    if (starvationSystem.accumulator >= starvationSystem.damageInterval) {
        starvationSystem.accumulator -= starvationSystem.damageInterval;

        // Apply starvation damage to hero
        for (size_t h = 0; h < hero.size(); ++h) {
            if (hero.stamina[h].current <= 0) {
                hero.health[h].change(-starvationSystem.damageAmount);
                if (hero.health[h].current <= 0) {
                    hero.delayedRemove.push_back(h);
                }
            }
        }

        // Apply starvation damage to NPCs
        for (size_t n = 0; n < npcs.size(); ++n) {
            if (npcs.stamina[n].current <= 0) {
                npcs.health[n].change(-starvationSystem.damageAmount);
                if (npcs.health[n].current <= 0) {
                    remove_npc(n);
                }
            }
        }
    }
}

void World::update_tiredness_system(float dt) {
    tirednessSystem.accumulator += dt;
    if (tirednessSystem.accumulator >= tirednessSystem.tirednessInterval) {
        tirednessSystem.accumulator -= tirednessSystem.tirednessInterval;

        // Decrease stamina for hero
        for (size_t h = 0; h < hero.size(); ++h) {
            hero.stamina[h].change(-tirednessSystem.tirednessAmount);
        }

        // Decrease stamina for NPCs
        for (size_t n = 0; n < npcs.size(); ++n) {
            npcs.stamina[n].change(-tirednessSystem.tirednessAmount);
        }
    }
}

void World::update_reproduction(float dt) {
    // Check for NPCs in the same location that can reproduce
    for (size_t i = 0; i < npcs.size(); ++i) {
        // Skip if already marked for removal
        bool iMarkedForRemoval = false;
        for (size_t idx : npcs.delayedRemove) {
            if (idx == i) {
                iMarkedForRemoval = true;
                break;
            }
        }
        if (iMarkedForRemoval)
            continue;

        // Check if this NPC is ready to reproduce (health > 90)
        if (npcs.health[i].current <= 90)
            continue;

        auto& transform_i = npcs.transform[i];
        auto& type_i = npcs.npcType[i];

        // Look for a mate
        for (size_t j = i + 1; j < npcs.size(); ++j) {
            // Skip if already marked for removal
            bool jMarkedForRemoval = false;
            for (size_t idx : npcs.delayedRemove) {
                if (idx == j) {
                    jMarkedForRemoval = true;
                    break;
                }
            }
            if (jMarkedForRemoval)
                continue;

            // Check if mate is ready to reproduce
            if (npcs.health[j].current <= 90)
                continue;

            auto& transform_j = npcs.transform[j];
            auto& type_j = npcs.npcType[j];

            // Check if same type
            bool sameType = false;
            if (std::holds_alternative<NPCConsumer>(type_i) && 
                std::holds_alternative<NPCConsumer>(type_j)) {
                sameType = true;
            } else if (std::holds_alternative<NPCPredator>(type_i) && 
                       std::holds_alternative<NPCPredator>(type_j)) {
                sameType = true;
            }

            if (!sameType)
                continue;

            // Check if in the same cell
            if (int(transform_i.x) == int(transform_j.x) &&
                int(transform_i.y) == int(transform_j.y)) {

                // Calculate offspring health (1/3 from each parent)
                int healthFromParent1 = npcs.health[i].current / 3;
                int healthFromParent2 = npcs.health[j].current / 3;
                int offspringHealth = healthFromParent1 + healthFromParent2;

                // Deduct health from parents
                npcs.health[i].change(-healthFromParent1);
                npcs.health[j].change(-healthFromParent2);

                // Create offspring of the same type
                Sprite offspringSprite = npcs.sprite[i]; // Use parent's sprite
                Transform2D offspringTransform = transform_i; // Same position

                // Add offspring
                npcs.sprite.push_back(offspringSprite);
                npcs.transform.push_back(offspringTransform);
                npcs.health.push_back(Health(offspringHealth));
                npcs.stamina.push_back(Stamina(100));
                npcs.restrictor.push_back(npcs.restrictor[i]);

                // Initialize NPCData with appropriate state based on type
                NPCData data;
                data.accumulatedTime = 0.f;
                data.targetPos = {-1, -1};
                data.state = std::holds_alternative<NPCConsumer>(type_i)
                    ? NPCState(ConsumerState::IDLE)
                    : NPCState(PredatorState::IDLE);
                npcs.npcData.push_back(data);
                npcs.npcType.push_back(type_i);

                // Each pair can only reproduce once per update
                break;
            }
        }
    }
}

void World::update_food_generator(float dt) {
    foodGenerator.timeSinceLastSpawn += dt;
    if (foodGenerator.timeSinceLastSpawn >= foodGenerator.spawnInterval) {
        foodGenerator.timeSinceLastSpawn -= foodGenerator.spawnInterval;

        if (!foodGenerator.dungeon || foodGenerator.foodTypes.empty())
            return;

        // Generate random food
        auto position = foodGenerator.dungeon->getRandomFloorPosition();

        // Select random food type based on weights
        int randValue = rand() % foodGenerator.totalWeight;
        for (const auto& foodTypeInfo : foodGenerator.foodTypes) {
            if (randValue < foodTypeInfo.weight) {
                add_food(foodTypeInfo.sprite, Transform2D(position.x, position.y), foodTypeInfo.foodType);
                break;
            }
            randValue -= foodTypeInfo.weight;
        }
    }
}

// ----------------------- thread safe version ------------------------

void World::process_deferred_removals_ts() {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    // Process hero removals
    if (!hero.delayedRemove.empty()) {
        std::sort(hero.delayedRemove.begin(), hero.delayedRemove.end(), std::greater<size_t>());
        for (size_t idx : hero.delayedRemove) {
            if (idx < hero.size()) {
                hero.sprite.erase(hero.sprite.begin() + idx);
                hero.transform.erase(hero.transform.begin() + idx);
                hero.health.erase(hero.health.begin() + idx);
                hero.stamina.erase(hero.stamina.begin() + idx);
                hero.restrictor.erase(hero.restrictor.begin() + idx);
                hero.heroData.erase(hero.heroData.begin() + idx);
            }
        }
        hero.delayedRemove.clear();
    }

    // Process NPC removals
    if (!npcs.delayedRemove.empty()) {
        std::sort(npcs.delayedRemove.begin(), npcs.delayedRemove.end(), std::greater<size_t>());
        for (size_t idx : npcs.delayedRemove) {
            if (idx < npcs.size()) {
                npcs.sprite.erase(npcs.sprite.begin() + idx);
                npcs.transform.erase(npcs.transform.begin() + idx);
                npcs.health.erase(npcs.health.begin() + idx);
                npcs.stamina.erase(npcs.stamina.begin() + idx);
                npcs.restrictor.erase(npcs.restrictor.begin() + idx);
                npcs.npcData.erase(npcs.npcData.begin() + idx);
                npcs.npcType.erase(npcs.npcType.begin() + idx);
            }
        }
        npcs.delayedRemove.clear();
    }

    // Process food removals
    if (!food.delayedRemove.empty()) {
        std::sort(food.delayedRemove.begin(), food.delayedRemove.end(), std::greater<size_t>());
        for (size_t idx : food.delayedRemove) {
            if (idx < food.size()) {
                food.sprite.erase(food.sprite.begin() + idx);
                food.transform.erase(food.transform.begin() + idx);
                food.foodType.erase(food.foodType.begin() + idx);
            }
        }
        food.delayedRemove.clear();
    }
}

void World::update_hero_ts(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    if (hero.size() == 0) return;

    const bool* keys = SDL_GetKeyboardState(nullptr);

    for (size_t i = 0; i < hero.size(); ++i) {
        auto& transform = hero.transform[i];
        auto& restrictor = hero.restrictor[i];
        auto& stamina = hero.stamina[i];
        auto& heroData = hero.heroData[i];

        const float cellPerSecond = stamina.get_speed();
        int2 intDelta{0, 0};
        bool moved = false;

        if (keys[SDL_SCANCODE_W]) { intDelta.y -= 1; moved = true; }
        if (keys[SDL_SCANCODE_S]) { intDelta.y += 1; moved = true; }
        if (keys[SDL_SCANCODE_A]) { intDelta.x -= 1; moved = true; }
        if (keys[SDL_SCANCODE_D]) { intDelta.x += 1; moved = true; }

        if (!moved)
            continue;

        if (moved && (heroData.timeSinceLastMove < 1.f / cellPerSecond)) {
            heroData.timeSinceLastMove += dt;
            continue;
        }

        heroData.timeSinceLastMove = 0.f;
        int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);

        if (restrictor->can_pass(newPos)) {
            transform.x += intDelta.x;
            transform.y += intDelta.y;
            // Bind camera to hero position
            if (heroData.cameraIndex < camera.size()) {
                camera.transform[heroData.cameraIndex].x = transform.x;
                camera.transform[heroData.cameraIndex].y = transform.y;
            }
        }
    }
}

void World::update_npcs_ts(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    const int2 directions[] = { int2{1,0}, int2{-1,0}, int2{0,1}, int2{0,-1} };

    for (size_t i = 0; i < npcs.size(); ++i) {
        auto& stamina = npcs.stamina[i];
        auto& npcData = npcs.npcData[i];

        npcData.accumulatedTime += dt * stamina.get_speed();
        if (npcData.accumulatedTime < 1.0f)
            continue;

        npcData.accumulatedTime -= 1.0f;

#if USE_BEHAVIOUR_TREE
        // Update Behaviour Tree based on NPC type
        if (std::holds_alternative<NPCConsumer>(npcs.npcType[i])) {
            update_consumer_bt(i, *this, directions);
        } else if (std::holds_alternative<NPCPredator>(npcs.npcType[i])) {
            update_predator_bt(i, *this, directions);
        }
#else
        // Update FSM based on NPC type
        if (std::holds_alternative<NPCConsumer>(npcs.npcType[i])) {
            update_consumer_fsm(i, *this, directions);
        } else if (std::holds_alternative<NPCPredator>(npcs.npcType[i])) {
            update_predator_fsm(i, *this, directions);
        }
#endif
    }
}

void World::update_food_consumption_ts(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    // Hero consumes food
    for (size_t h = 0; h < hero.size(); ++h) {
        bool heroMarkedForRemoval = false;
        for (size_t idx : hero.delayedRemove) {
            if (idx == h) {
                heroMarkedForRemoval = true;
                break;
            }
        }
        if (heroMarkedForRemoval)
            continue;

        auto& heroTransform = hero.transform[h];
        auto& heroHealth = hero.health[h];
        auto& heroStamina = hero.stamina[h];

        for (size_t f = 0; f < food.size(); ++f) {
            bool foodMarkedForRemoval = false;
            for (size_t idx : food.delayedRemove) {
                if (idx == f) {
                    foodMarkedForRemoval = true;
                    break;
                }
            }
            if (foodMarkedForRemoval)
                continue;

            auto& foodTransform = food.transform[f];

            // Check collision
            if (int(heroTransform.x) == int(foodTransform.x) &&
                int(heroTransform.y) == int(foodTransform.y)) {
                // Apply food effect
                std::visit([&](auto&& foodType) {
                    using T = std::decay_t<decltype(foodType)>;
                    if constexpr (std::is_same_v<T, HealthFood>) {
                        heroHealth.change(foodType.restore);
                    } else if constexpr (std::is_same_v<T, StaminaFood>) {
                        heroStamina.change(foodType.restore);
                    }
                }, food.foodType[f]);

                remove_food(f);
                break;
            }
        }
    }

    // NPCs consume food
    for (size_t n = 0; n < npcs.size(); ++n) {
        // Skip NPC if already marked for removal
        bool npcMarkedForRemoval = false;
        for (size_t idx : npcs.delayedRemove) {
            if (idx == n) {
                npcMarkedForRemoval = true;
                break;
            }
        }
        if (npcMarkedForRemoval)
            continue;

        auto& npcTransform = npcs.transform[n];
        auto& npcHealth = npcs.health[n];
        auto& npcStamina = npcs.stamina[n];

        for (size_t f = 0; f < food.size(); ++f) {
            // Skip food if already marked for removal
            bool foodMarkedForRemoval = false;
            for (size_t idx : food.delayedRemove) {
                if (idx == f) {
                    foodMarkedForRemoval = true;
                    break;
                }
            }
            if (foodMarkedForRemoval)
                continue;

            auto& foodTransform = food.transform[f];

            // Check collision
            if (int(npcTransform.x) == int(foodTransform.x) &&
                int(npcTransform.y) == int(foodTransform.y)) {
                // Apply food effect
                std::visit([&](auto&& foodType) {
                    using T = std::decay_t<decltype(foodType)>;
                    if constexpr (std::is_same_v<T, HealthFood>) {
                        npcHealth.change(foodType.restore);
                    } else if constexpr (std::is_same_v<T, StaminaFood>) {
                        npcStamina.change(foodType.restore);
                    }
                }, food.foodType[f]);

                remove_food(f);
                break;
            }
        }
    }
}

void World::update_predators_ts(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    // Predators hunt consumer NPCs and hero
    for (size_t p = 0; p < npcs.size(); ++p) {
        // Check if this NPC is a predator
        if (!std::holds_alternative<NPCPredator>(npcs.npcType[p]))
            continue;

        // Check if this predator was already marked for removal
        bool predatorMarkedForRemoval = false;
        for (size_t idx : npcs.delayedRemove) {
            if (idx == p) {
                predatorMarkedForRemoval = true;
                break;
            }
        }
        if (predatorMarkedForRemoval)
            continue;
        auto& predatorTransform = npcs.transform[p];
        auto& predatorHealth = npcs.health[p];
        bool victimFound = false;
        // First, check if hero can be hunted
        for (size_t h = 0; h < hero.size(); ++h) {
            // Check if hero is already marked for removal
            bool heroMarkedForRemoval = false;
            for (size_t idx : hero.delayedRemove) {
                if (idx == h) {
                    heroMarkedForRemoval = true;
                    break;
                }
            }
            if (heroMarkedForRemoval)
                continue;
            auto& heroTransform = hero.transform[h];
            auto& heroHealth = hero.health[h];
            // Check collision
            if (int(predatorTransform.x) == int(heroTransform.x) &&
                int(predatorTransform.y) == int(heroTransform.y)) {
                // Heal predator by hero's current health
                predatorHealth.change(heroHealth.current);
                // Kill hero
                hero.delayedRemove.push_back(h);
                victimFound = true;
                break;
            }
        }
        if (victimFound)
            continue; // Predator already ate, skip to next predator
        // Look for victims (consumer NPCs)
        for (size_t v = 0; v < npcs.size(); ++v) {
            if (p == v) continue; // Skip self
            // Check if victim is already marked for removal
            bool victimMarkedForRemoval = false;
            for (size_t idx : npcs.delayedRemove) {
                if (idx == v) {
                    victimMarkedForRemoval = true;
                    break;
                }
            }
            if (victimMarkedForRemoval)
                continue;
            // Check if victim is a consumer
            if (!std::holds_alternative<NPCConsumer>(npcs.npcType[v]))
                continue;
            auto& victimTransform = npcs.transform[v];
            auto& victimHealth = npcs.health[v];
            // Check collision
            if (int(predatorTransform.x) == int(victimTransform.x) &&
                int(predatorTransform.y) == int(victimTransform.y)) {
                // Heal predator by victim's current health
                predatorHealth.change(victimHealth.current);
                // Kill victim
                remove_npc(v);
                break; // Consume only one victim at a time
            }
        }
    }
}

void World::update_starvation_system_ts(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    starvationSystem.accumulator += dt;
    if (starvationSystem.accumulator >= starvationSystem.damageInterval) {
        starvationSystem.accumulator -= starvationSystem.damageInterval;

        // Apply starvation damage to hero
        for (size_t h = 0; h < hero.size(); ++h) {
            if (hero.stamina[h].current <= 0) {
                hero.health[h].change(-starvationSystem.damageAmount);
                if (hero.health[h].current <= 0) {
                    hero.delayedRemove.push_back(h);
                }
            }
        }

        // Apply starvation damage to NPCs
        for (size_t n = 0; n < npcs.size(); ++n) {
            if (npcs.stamina[n].current <= 0) {
                npcs.health[n].change(-starvationSystem.damageAmount);
                if (npcs.health[n].current <= 0) {
                    remove_npc(n);
                }
            }
        }
    }
}

void World::update_tiredness_system_ts(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    tirednessSystem.accumulator += dt;
    if (tirednessSystem.accumulator >= tirednessSystem.tirednessInterval) {
        tirednessSystem.accumulator -= tirednessSystem.tirednessInterval;

        // Decrease stamina for hero
        for (size_t h = 0; h < hero.size(); ++h) {
            hero.stamina[h].change(-tirednessSystem.tirednessAmount);
        }

        // Decrease stamina for NPCs
        for (size_t n = 0; n < npcs.size(); ++n) {
            npcs.stamina[n].change(-tirednessSystem.tirednessAmount);
        }
    }
}

void World::update_reproduction_ts(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    // Check for NPCs in the same location that can reproduce
    for (size_t i = 0; i < npcs.size(); ++i) {
        // Skip if already marked for removal
        bool iMarkedForRemoval = false;
        for (size_t idx : npcs.delayedRemove) {
            if (idx == i) {
                iMarkedForRemoval = true;
                break;
            }
        }
        if (iMarkedForRemoval)
            continue;

        // Check if this NPC is ready to reproduce (health > 90)
        if (npcs.health[i].current <= 90)
            continue;

        auto& transform_i = npcs.transform[i];
        auto& type_i = npcs.npcType[i];

        // Look for a mate
        for (size_t j = i + 1; j < npcs.size(); ++j) {
            // Skip if already marked for removal
            bool jMarkedForRemoval = false;
            for (size_t idx : npcs.delayedRemove) {
                if (idx == j) {
                    jMarkedForRemoval = true;
                    break;
                }
            }
            if (jMarkedForRemoval)
                continue;

            // Check if mate is ready to reproduce
            if (npcs.health[j].current <= 90)
                continue;

            auto& transform_j = npcs.transform[j];
            auto& type_j = npcs.npcType[j];

            // Check if same type
            bool sameType = false;
            if (std::holds_alternative<NPCConsumer>(type_i) &&
                std::holds_alternative<NPCConsumer>(type_j)) {
                sameType = true;
            } else if (std::holds_alternative<NPCPredator>(type_i) &&
                       std::holds_alternative<NPCPredator>(type_j)) {
                sameType = true;
            }

            if (!sameType)
                continue;

            // Check if in the same cell
            if (int(transform_i.x) == int(transform_j.x) &&
                int(transform_i.y) == int(transform_j.y)) {

                // Calculate offspring health (1/3 from each parent)
                int healthFromParent1 = npcs.health[i].current / 3;
                int healthFromParent2 = npcs.health[j].current / 3;
                int offspringHealth = healthFromParent1 + healthFromParent2;

                // Deduct health from parents
                npcs.health[i].change(-healthFromParent1);
                npcs.health[j].change(-healthFromParent2);

                // Create offspring of the same type
                Sprite offspringSprite = npcs.sprite[i]; // Use parent's sprite
                Transform2D offspringTransform = transform_i; // Same position

                // Add offspring
                npcs.sprite.push_back(offspringSprite);
                npcs.transform.push_back(offspringTransform);
                npcs.health.push_back(Health(offspringHealth));
                npcs.stamina.push_back(Stamina(100));
                npcs.restrictor.push_back(npcs.restrictor[i]);

                // Initialize NPCData with appropriate state based on type
                NPCData data;
                data.accumulatedTime = 0.f;
                data.targetPos = {-1, -1};
                data.state = std::holds_alternative<NPCConsumer>(type_i)
                    ? NPCState(ConsumerState::IDLE)
                    : NPCState(PredatorState::IDLE);
                npcs.npcData.push_back(data);
                npcs.npcType.push_back(type_i);

                // Each pair can only reproduce once per update
                break;
            }
        }
    }
}

void World::update_food_generator_ts(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    foodGenerator.timeSinceLastSpawn += dt;
    if (foodGenerator.timeSinceLastSpawn >= foodGenerator.spawnInterval) {
        foodGenerator.timeSinceLastSpawn -= foodGenerator.spawnInterval;

        if (!foodGenerator.dungeon || foodGenerator.foodTypes.empty())
            return;

        // Generate random food
        auto position = foodGenerator.dungeon->getRandomFloorPosition();

        // Select random food type based on weights
        int randValue = rand() % foodGenerator.totalWeight;
        for (const auto& foodTypeInfo : foodGenerator.foodTypes) {
            if (randValue < foodTypeInfo.weight) {
                add_food(foodTypeInfo.sprite, Transform2D(position.x, position.y), foodTypeInfo.foodType);
                break;
            }
            randValue -= foodTypeInfo.weight;
        }
    }
}

// ----------------------- thread pool version ------------------------


void World::process_deferred_removals_tp() {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    // Process hero removals
    if (!hero.delayedRemove.empty()) {
        std::sort(hero.delayedRemove.begin(), hero.delayedRemove.end(), std::greater<size_t>());
        for (size_t idx : hero.delayedRemove) {
            if (idx < hero.size()) {
                hero.sprite.erase(hero.sprite.begin() + idx);
                hero.transform.erase(hero.transform.begin() + idx);
                hero.health.erase(hero.health.begin() + idx);
                hero.stamina.erase(hero.stamina.begin() + idx);
                hero.restrictor.erase(hero.restrictor.begin() + idx);
                hero.heroData.erase(hero.heroData.begin() + idx);
            }
        }
        hero.delayedRemove.clear();
    }

    // Process NPC removals
    if (!npcs.delayedRemove.empty()) {
        std::sort(npcs.delayedRemove.begin(), npcs.delayedRemove.end(), std::greater<size_t>());
        for (size_t idx : npcs.delayedRemove) {
            if (idx < npcs.size()) {
                npcs.sprite.erase(npcs.sprite.begin() + idx);
                npcs.transform.erase(npcs.transform.begin() + idx);
                npcs.health.erase(npcs.health.begin() + idx);
                npcs.stamina.erase(npcs.stamina.begin() + idx);
                npcs.restrictor.erase(npcs.restrictor.begin() + idx);
                npcs.npcData.erase(npcs.npcData.begin() + idx);
                npcs.npcType.erase(npcs.npcType.begin() + idx);
            }
        }
        npcs.delayedRemove.clear();
    }

    // Process food removals
    if (!food.delayedRemove.empty()) {
        std::sort(food.delayedRemove.begin(), food.delayedRemove.end(), std::greater<size_t>());
        for (size_t idx : food.delayedRemove) {
            if (idx < food.size()) {
                food.sprite.erase(food.sprite.begin() + idx);
                food.transform.erase(food.transform.begin() + idx);
                food.foodType.erase(food.foodType.begin() + idx);
            }
        }
        food.delayedRemove.clear();
    }
}

void World::update_hero_tp(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    if (hero.size() == 0) return;

    const bool* keys = SDL_GetKeyboardState(nullptr);

    for (size_t i = 0; i < hero.size(); ++i) {
        auto& transform = hero.transform[i];
        auto& restrictor = hero.restrictor[i];
        auto& stamina = hero.stamina[i];
        auto& heroData = hero.heroData[i];

        const float cellPerSecond = stamina.get_speed();
        int2 intDelta{0, 0};
        bool moved = false;

        if (keys[SDL_SCANCODE_W]) { intDelta.y -= 1; moved = true; }
        if (keys[SDL_SCANCODE_S]) { intDelta.y += 1; moved = true; }
        if (keys[SDL_SCANCODE_A]) { intDelta.x -= 1; moved = true; }
        if (keys[SDL_SCANCODE_D]) { intDelta.x += 1; moved = true; }

        if (!moved)
            continue;

        if (moved && (heroData.timeSinceLastMove < 1.f / cellPerSecond)) {
            heroData.timeSinceLastMove += dt;
            continue;
        }

        heroData.timeSinceLastMove = 0.f;
        int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);

        if (restrictor->can_pass(newPos)) {
            transform.x += intDelta.x;
            transform.y += intDelta.y;
            // Bind camera to hero position
            if (heroData.cameraIndex < camera.size()) {
                camera.transform[heroData.cameraIndex].x = transform.x;
                camera.transform[heroData.cameraIndex].y = transform.y;
            }
        }
    }
}

void World::update_npcs_tp(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    const int2 directions[] = { int2{1,0}, int2{-1,0}, int2{0,1}, int2{0,-1} };

    for (size_t i = 0; i < npcs.size(); ++i) {
        auto& stamina = npcs.stamina[i];
        auto& npcData = npcs.npcData[i];

        npcData.accumulatedTime += dt * stamina.get_speed();
        if (npcData.accumulatedTime < 1.0f)
            continue;

        npcData.accumulatedTime -= 1.0f;

#if USE_BEHAVIOUR_TREE
        // Update Behaviour Tree based on NPC type
        if (std::holds_alternative<NPCConsumer>(npcs.npcType[i])) {
            update_consumer_bt(i, *this, directions);
        } else if (std::holds_alternative<NPCPredator>(npcs.npcType[i])) {
            update_predator_bt(i, *this, directions);
        }
#else
        // Update FSM based on NPC type
        if (std::holds_alternative<NPCConsumer>(npcs.npcType[i])) {
            update_consumer_fsm(i, *this, directions);
        } else if (std::holds_alternative<NPCPredator>(npcs.npcType[i])) {
            update_predator_fsm(i, *this, directions);
        }
#endif
    }
}

void World::update_food_consumption_tp(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    // Hero consumes food
    for (size_t h = 0; h < hero.size(); ++h) {
        bool heroMarkedForRemoval = false;
        for (size_t idx : hero.delayedRemove) {
            if (idx == h) {
                heroMarkedForRemoval = true;
                break;
            }
        }
        if (heroMarkedForRemoval)
            continue;

        auto& heroTransform = hero.transform[h];
        auto& heroHealth = hero.health[h];
        auto& heroStamina = hero.stamina[h];

        for (size_t f = 0; f < food.size(); ++f) {
            bool foodMarkedForRemoval = false;
            for (size_t idx : food.delayedRemove) {
                if (idx == f) {
                    foodMarkedForRemoval = true;
                    break;
                }
            }
            if (foodMarkedForRemoval)
                continue;

            auto& foodTransform = food.transform[f];

            // Check collision
            if (int(heroTransform.x) == int(foodTransform.x) &&
                int(heroTransform.y) == int(foodTransform.y)) {
                // Apply food effect
                std::visit([&](auto&& foodType) {
                    using T = std::decay_t<decltype(foodType)>;
                    if constexpr (std::is_same_v<T, HealthFood>) {
                        heroHealth.change(foodType.restore);
                    } else if constexpr (std::is_same_v<T, StaminaFood>) {
                        heroStamina.change(foodType.restore);
                    }
                }, food.foodType[f]);

                remove_food(f);
                break;
            }
        }
    }

    // NPCs consume food
    for (size_t n = 0; n < npcs.size(); ++n) {
        // Skip NPC if already marked for removal
        bool npcMarkedForRemoval = false;
        for (size_t idx : npcs.delayedRemove) {
            if (idx == n) {
                npcMarkedForRemoval = true;
                break;
            }
        }
        if (npcMarkedForRemoval)
            continue;

        auto& npcTransform = npcs.transform[n];
        auto& npcHealth = npcs.health[n];
        auto& npcStamina = npcs.stamina[n];

        for (size_t f = 0; f < food.size(); ++f) {
            // Skip food if already marked for removal
            bool foodMarkedForRemoval = false;
            for (size_t idx : food.delayedRemove) {
                if (idx == f) {
                    foodMarkedForRemoval = true;
                    break;
                }
            }
            if (foodMarkedForRemoval)
                continue;

            auto& foodTransform = food.transform[f];

            // Check collision
            if (int(npcTransform.x) == int(foodTransform.x) &&
                int(npcTransform.y) == int(foodTransform.y)) {
                // Apply food effect
                std::visit([&](auto&& foodType) {
                    using T = std::decay_t<decltype(foodType)>;
                    if constexpr (std::is_same_v<T, HealthFood>) {
                        npcHealth.change(foodType.restore);
                    } else if constexpr (std::is_same_v<T, StaminaFood>) {
                        npcStamina.change(foodType.restore);
                    }
                }, food.foodType[f]);

                remove_food(f);
                break;
            }
        }
    }
}

void World::update_predators_tp(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    // Predators hunt consumer NPCs and hero
    for (size_t p = 0; p < npcs.size(); ++p) {
        // Check if this NPC is a predator
        if (!std::holds_alternative<NPCPredator>(npcs.npcType[p]))
            continue;

        // Check if this predator was already marked for removal
        bool predatorMarkedForRemoval = false;
        for (size_t idx : npcs.delayedRemove) {
            if (idx == p) {
                predatorMarkedForRemoval = true;
                break;
            }
        }
        if (predatorMarkedForRemoval)
            continue;
        auto& predatorTransform = npcs.transform[p];
        auto& predatorHealth = npcs.health[p];
        bool victimFound = false;
        // First, check if hero can be hunted
        for (size_t h = 0; h < hero.size(); ++h) {
            // Check if hero is already marked for removal
            bool heroMarkedForRemoval = false;
            for (size_t idx : hero.delayedRemove) {
                if (idx == h) {
                    heroMarkedForRemoval = true;
                    break;
                }
            }
            if (heroMarkedForRemoval)
                continue;
            auto& heroTransform = hero.transform[h];
            auto& heroHealth = hero.health[h];
            // Check collision
            if (int(predatorTransform.x) == int(heroTransform.x) &&
                int(predatorTransform.y) == int(heroTransform.y)) {
                // Heal predator by hero's current health
                predatorHealth.change(heroHealth.current);
                // Kill hero
                hero.delayedRemove.push_back(h);
                victimFound = true;
                break;
            }
        }
        if (victimFound)
            continue; // Predator already ate, skip to next predator
        // Look for victims (consumer NPCs)
        for (size_t v = 0; v < npcs.size(); ++v) {
            if (p == v) continue; // Skip self
            // Check if victim is already marked for removal
            bool victimMarkedForRemoval = false;
            for (size_t idx : npcs.delayedRemove) {
                if (idx == v) {
                    victimMarkedForRemoval = true;
                    break;
                }
            }
            if (victimMarkedForRemoval)
                continue;
            // Check if victim is a consumer
            if (!std::holds_alternative<NPCConsumer>(npcs.npcType[v]))
                continue;
            auto& victimTransform = npcs.transform[v];
            auto& victimHealth = npcs.health[v];
            // Check collision
            if (int(predatorTransform.x) == int(victimTransform.x) &&
                int(predatorTransform.y) == int(victimTransform.y)) {
                // Heal predator by victim's current health
                predatorHealth.change(victimHealth.current);
                // Kill victim
                remove_npc(v);
                break; // Consume only one victim at a time
            }
        }
    }
}

void World::update_starvation_system_tp(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    starvationSystem.accumulator += dt;
    if (starvationSystem.accumulator >= starvationSystem.damageInterval) {
        starvationSystem.accumulator -= starvationSystem.damageInterval;

        // Apply starvation damage to hero
        for (size_t h = 0; h < hero.size(); ++h) {
            if (hero.stamina[h].current <= 0) {
                hero.health[h].change(-starvationSystem.damageAmount);
                if (hero.health[h].current <= 0) {
                    hero.delayedRemove.push_back(h);
                }
            }
        }

        // Apply starvation damage to NPCs
        for (size_t n = 0; n < npcs.size(); ++n) {
            if (npcs.stamina[n].current <= 0) {
                npcs.health[n].change(-starvationSystem.damageAmount);
                if (npcs.health[n].current <= 0) {
                    remove_npc(n);
                }
            }
        }
    }
}

void World::update_tiredness_system_tp(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    tirednessSystem.accumulator += dt;
    if (tirednessSystem.accumulator >= tirednessSystem.tirednessInterval) {
        tirednessSystem.accumulator -= tirednessSystem.tirednessInterval;

        // Decrease stamina for hero
        for (size_t h = 0; h < hero.size(); ++h) {
            hero.stamina[h].change(-tirednessSystem.tirednessAmount);
        }

        // Decrease stamina for NPCs
        for (size_t n = 0; n < npcs.size(); ++n) {
            npcs.stamina[n].change(-tirednessSystem.tirednessAmount);
        }
    }
}

void World::update_reproduction_tp(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    // Check for NPCs in the same location that can reproduce
    for (size_t i = 0; i < npcs.size(); ++i) {
        // Skip if already marked for removal
        bool iMarkedForRemoval = false;
        for (size_t idx : npcs.delayedRemove) {
            if (idx == i) {
                iMarkedForRemoval = true;
                break;
            }
        }
        if (iMarkedForRemoval)
            continue;

        // Check if this NPC is ready to reproduce (health > 90)
        if (npcs.health[i].current <= 90)
            continue;

        auto& transform_i = npcs.transform[i];
        auto& type_i = npcs.npcType[i];

        // Look for a mate
        for (size_t j = i + 1; j < npcs.size(); ++j) {
            // Skip if already marked for removal
            bool jMarkedForRemoval = false;
            for (size_t idx : npcs.delayedRemove) {
                if (idx == j) {
                    jMarkedForRemoval = true;
                    break;
                }
            }
            if (jMarkedForRemoval)
                continue;

            // Check if mate is ready to reproduce
            if (npcs.health[j].current <= 90)
                continue;

            auto& transform_j = npcs.transform[j];
            auto& type_j = npcs.npcType[j];

            // Check if same type
            bool sameType = false;
            if (std::holds_alternative<NPCConsumer>(type_i) &&
                std::holds_alternative<NPCConsumer>(type_j)) {
                sameType = true;
            } else if (std::holds_alternative<NPCPredator>(type_i) &&
                       std::holds_alternative<NPCPredator>(type_j)) {
                sameType = true;
            }

            if (!sameType)
                continue;

            // Check if in the same cell
            if (int(transform_i.x) == int(transform_j.x) &&
                int(transform_i.y) == int(transform_j.y)) {

                // Calculate offspring health (1/3 from each parent)
                int healthFromParent1 = npcs.health[i].current / 3;
                int healthFromParent2 = npcs.health[j].current / 3;
                int offspringHealth = healthFromParent1 + healthFromParent2;

                // Deduct health from parents
                npcs.health[i].change(-healthFromParent1);
                npcs.health[j].change(-healthFromParent2);

                // Create offspring of the same type
                Sprite offspringSprite = npcs.sprite[i]; // Use parent's sprite
                Transform2D offspringTransform = transform_i; // Same position

                // Add offspring
                npcs.sprite.push_back(offspringSprite);
                npcs.transform.push_back(offspringTransform);
                npcs.health.push_back(Health(offspringHealth));
                npcs.stamina.push_back(Stamina(100));
                npcs.restrictor.push_back(npcs.restrictor[i]);

                // Initialize NPCData with appropriate state based on type
                NPCData data;
                data.accumulatedTime = 0.f;
                data.targetPos = {-1, -1};
                data.state = std::holds_alternative<NPCConsumer>(type_i)
                    ? NPCState(ConsumerState::IDLE)
                    : NPCState(PredatorState::IDLE);
                npcs.npcData.push_back(data);
                npcs.npcType.push_back(type_i);

                // Each pair can only reproduce once per update
                break;
            }
        }
    }
}

void World::update_food_generator_tp(float dt) {
    std::lock_guard<std::mutex> lock(g_worldMutex);
    foodGenerator.timeSinceLastSpawn += dt;
    if (foodGenerator.timeSinceLastSpawn >= foodGenerator.spawnInterval) {
        foodGenerator.timeSinceLastSpawn -= foodGenerator.spawnInterval;

        if (!foodGenerator.dungeon || foodGenerator.foodTypes.empty())
            return;

        // Generate random food
        auto position = foodGenerator.dungeon->getRandomFloorPosition();

        // Select random food type based on weights
        int randValue = rand() % foodGenerator.totalWeight;
        for (const auto& foodTypeInfo : foodGenerator.foodTypes) {
            if (randValue < foodTypeInfo.weight) {
                add_food(foodTypeInfo.sprite, Transform2D(position.x, position.y), foodTypeInfo.foodType);
                break;
            }
            randValue -= foodTypeInfo.weight;
        }
    }
}