#include "world.h"
#include "dungeon_generator.h"
#include "dungeon_restrictor.h"
#include <SDL3/SDL.h>
#include <algorithm>

void World::update(float dt) {
    // Process deferred removals first
    process_deferred_removals();

    // Update all systems
    update_hero(dt);
    update_npcs(dt);
    update_food_consumption(dt);
    update_predators(dt);
    update_starvation_system(dt);
    update_tiredness_system(dt);
    update_food_generator(dt);
}

void World::process_deferred_removals() {
    // Process Hero removals
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

    // Process NPC removals (sort in descending order to remove from back to front)
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

    // Process Food removals
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
        auto& transform = npcs.transform[i];
        auto& restrictor = npcs.restrictor[i];
        auto& stamina = npcs.stamina[i];
        auto& npcData = npcs.npcData[i];

        npcData.accumulatedTime += dt * stamina.get_speed();
        if (npcData.accumulatedTime < 1.0f)
            continue;

        npcData.accumulatedTime -= 1.0f;

        // Try to move in a random direction
        int dirIdx = rand() % 4;
        int2 intDelta = directions[dirIdx];
        int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);

        if (restrictor->can_pass(newPos)) {
            transform.x += intDelta.x;
            transform.y += intDelta.y;
        }
    }
}

void World::update_food_consumption(float dt) {
    if (hero.size() == 0) return;

    // Hero consumes food
    for (size_t h = 0; h < hero.size(); ++h) {
        auto& heroTransform = hero.transform[h];
        auto& heroHealth = hero.health[h];
        auto& heroStamina = hero.stamina[h];
        for (size_t f = 0; f < food.size(); ++f) {
            // Check if food is already marked for removal
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
                // Consume food based on type
                std::visit([&](auto&& foodData) {
                    using T = std::decay_t<decltype(foodData)>;
                    if constexpr (std::is_same_v<T, HealthFood>) {
                        heroHealth.change(foodData.restore);
                    } else if constexpr (std::is_same_v<T, StaminaFood>) {
                        heroStamina.change(foodData.restore);
                    }
                }, food.foodType[f]);
                // Mark food for removal
                remove_food(f);
                break; // Consume only one food at a time
            }
        }
    }

    // NPCs consume food (only consumers, not predators)
    for (size_t n = 0; n < npcs.size(); ++n) {
        // Check if this NPC is a consumer
        if (!std::holds_alternative<NPCConsumer>(npcs.npcType[n]))
            continue;
        
        // Check if this NPC is already marked for removal
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
            // Check if food is already marked for removal
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
                
                // Consume food based on type
                std::visit([&](auto&& foodData) {
                    using T = std::decay_t<decltype(foodData)>;
                    if constexpr (std::is_same_v<T, HealthFood>) {
                        npcHealth.change(foodData.restore);
                    } else if constexpr (std::is_same_v<T, StaminaFood>) {
                        npcStamina.change(foodData.restore);
                    }
                }, food.foodType[f]);
                
                // Mark food for removal
                remove_food(f);
                break; // Consume only one food at a time
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
    if (starvationSystem.accumulator < starvationSystem.damageInterval)
        return;

    starvationSystem.accumulator -= starvationSystem.damageInterval;

    // Damage hero
    for (size_t i = 0; i < hero.size(); ++i) {
        hero.health[i].change(-starvationSystem.damageAmount);
        if (hero.health[i].current <= 0) {
            // Hero died - mark for removal
            hero.delayedRemove.push_back(i);
        }
    }

    // Damage NPCs
    for (size_t i = 0; i < npcs.size(); ++i) {
        npcs.health[i].change(-starvationSystem.damageAmount);
        if (npcs.health[i].current <= 0) {
            remove_npc(i);
        }
    }
}

void World::update_tiredness_system(float dt) {
    tirednessSystem.accumulator += dt;
    if (tirednessSystem.accumulator < tirednessSystem.tirednessInterval)
        return;

    tirednessSystem.accumulator -= tirednessSystem.tirednessInterval;

    // Drain stamina from hero
    for (size_t i = 0; i < hero.size(); ++i) {
        hero.stamina[i].change(-tirednessSystem.tirednessAmount);
    }

    // Drain stamina from NPCs
    for (size_t i = 0; i < npcs.size(); ++i) {
        npcs.stamina[i].change(-tirednessSystem.tirednessAmount);
    }
}

void World::update_food_generator(float dt) {
    foodGenerator.timeSinceLastSpawn += dt;
    if (foodGenerator.timeSinceLastSpawn < foodGenerator.spawnInterval)
        return;

    foodGenerator.timeSinceLastSpawn = 0.f;

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