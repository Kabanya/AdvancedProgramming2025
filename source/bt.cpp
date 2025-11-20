#include "bt.h"
#include "pathfinding.h"
#include "dungeon_restrictor.h"
#include <cstdlib>

#define ENABLE_NPC_PATHFINDING 1

BTStatus update_consumer_bt(size_t npc_index, World& world, const int2 directions[4]) {
    BTContext ctx;
    ctx.world = &world;
    ctx.npc_index = npc_index;
    ctx.directions = directions;
    ctx.moved = false;

    // Condition: Check if predator is nearby
    auto check_predator = Condition([](BTContext& ctx) -> bool {
        int2 predator_pos;
        if (is_predator_nearby(ctx, predator_pos)) {
            ctx.world->npcs.npcData[ctx.npc_index].targetPos = predator_pos;
            return true;
        }
        return false;
    });

    // Action: Flee from predator
    auto flee_action = Action([](BTContext& ctx) {
        auto& transform = ctx.world->npcs.transform[ctx.npc_index];
        auto& restrictor = ctx.world->npcs.restrictor[ctx.npc_index];
        int2 predator_pos = ctx.world->npcs.npcData[ctx.npc_index].targetPos;

#if ENABLE_NPC_PATHFINDING
        int2 currentPos = {(int)transform.x, (int)transform.y};
        int2 fleeDir = {
            currentPos.x - predator_pos.x,
            currentPos.y - predator_pos.y
        };
        // Normalize flee direction
        if (fleeDir.x != 0) fleeDir.x = fleeDir.x > 0 ? 1 : -1;
        if (fleeDir.y != 0) fleeDir.y = fleeDir.y > 0 ? 1 : -1;
        // Try to move in flee direction
        if (fleeDir.x != 0 || fleeDir.y != 0) {
            int2 newPos = {currentPos.x + fleeDir.x, currentPos.y + fleeDir.y};
            if (restrictor->can_pass(newPos)) {
                transform.x = newPos.x;
                transform.y = newPos.y;
                ctx.moved = true;
            } else {
                // Try perpendicular directions
                int2 altPos1 = {currentPos.x + fleeDir.y, currentPos.y};
                int2 altPos2 = {currentPos.x, currentPos.y + fleeDir.x};
                if (restrictor->can_pass(altPos1)) {
                    transform.x = altPos1.x;
                    transform.y = altPos1.y;
                    ctx.moved = true;
                } else if (restrictor->can_pass(altPos2)) {
                    transform.x = altPos2.x;
                    transform.y = altPos2.y;
                    ctx.moved = true;
                }
            }
        }
#endif
    });

    // Sequence: Flee behavior
    auto flee_behavior = Sequence(check_predator, flee_action);

    // Condition: Check if ready to reproduce
    auto check_ready_reproduce = Condition([](BTContext& ctx) -> bool {
        return ready_to_reproduce(ctx);
    });

    // Action: Seek mate
    auto seek_mate_action = Action([](BTContext& ctx) {
        auto& transform = ctx.world->npcs.transform[ctx.npc_index];
        auto& restrictor = ctx.world->npcs.restrictor[ctx.npc_index];
#if ENABLE_NPC_PATHFINDING
        int2 mate_pos;
        size_t mate_index;
        if (find_mate(ctx, mate_pos, mate_index)) {
            int2 start = {(int)transform.x, (int)transform.y};
            int2 goal = mate_pos;
            auto path = astar(start, goal,
                [&](const int2& pos) { return restrictor->can_pass(pos); });
            if (path.size() > 1) {
                transform.x = path[1].x;
                transform.y = path[1].y;
                ctx.moved = true;
            } else {
                // Fallback: random movement if path not found
                int dirIdx = rand() % 4;
                int2 intDelta = ctx.directions[dirIdx];
                int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);
                if (restrictor->can_pass(newPos)) {
                    transform.x += intDelta.x;
                    transform.y += intDelta.y;
                    ctx.moved = true;
                }
            }
        }
#endif
    });

    // Sequence: Reproduction behavior
    auto reproduction_behavior = Sequence(check_ready_reproduce, seek_mate_action);

    // Condition: Check if hungry
    auto check_hungry = Condition([](BTContext& ctx) -> bool {
        return is_npc_hungry(ctx);
    });

    // Action: Seek food
    auto seek_food_action = Action([](BTContext& ctx) {
        auto& transform = ctx.world->npcs.transform[ctx.npc_index];
        auto& restrictor = ctx.world->npcs.restrictor[ctx.npc_index];
#if ENABLE_NPC_PATHFINDING
        // Find nearest food
        size_t closestFood = (size_t)-1;
        float minDist = FLT_MAX;
        for (size_t f = 0; f < ctx.world->food.size(); ++f) {
            float dist = std::abs(transform.x - ctx.world->food.transform[f].x) +
                        std::abs(transform.y - ctx.world->food.transform[f].y);
            if (dist < minDist) {
                minDist = dist;
                closestFood = f;
            }
        }

        if (closestFood != (size_t)-1) {
            int2 start = {(int)transform.x, (int)transform.y};
            int2 goal = {(int)ctx.world->food.transform[closestFood].x,
                        (int)ctx.world->food.transform[closestFood].y};
            auto path = astar(start, goal,
                [&](const int2& pos) { return restrictor->can_pass(pos); },
                [&](const int2& pos) {
                    // Avoid predators in pathfinding
                    for (size_t p = 0; p < ctx.world->npcs.size(); ++p) {
                        if (std::holds_alternative<NPCPredator>(ctx.world->npcs.npcType[p]) &&
                            int(ctx.world->npcs.transform[p].x) == pos.x &&
                            int(ctx.world->npcs.transform[p].y) == pos.y) {
                            return false;
                        }
                    }
                    return true;
                });
            if (path.size() > 1) {
                transform.x = path[1].x;
                transform.y = path[1].y;
                ctx.moved = true;
            }
        }
#endif
    });

    // Sequence: Seek food behavior
    auto seek_food_behavior = Sequence(check_hungry, seek_food_action);

    // Action: Idle (random movement)
    auto idle_behavior = Action([](BTContext& ctx) {
        if (!ctx.moved) {
            auto& transform = ctx.world->npcs.transform[ctx.npc_index];
            auto& restrictor = ctx.world->npcs.restrictor[ctx.npc_index];
            int dirIdx = rand() % 4;
            int2 intDelta = ctx.directions[dirIdx];
            int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);
            if (restrictor->can_pass(newPos)) {
                transform.x += intDelta.x;
                transform.y += intDelta.y;
            }
        }
    });

    // Main tree: Selector with priority behaviors
    auto consumer_tree = Selector(
        flee_behavior,
        reproduction_behavior,
        seek_food_behavior,
        idle_behavior
    );

    return consumer_tree.tick(ctx);
}

BTStatus update_predator_bt(size_t npc_index, World& world, const int2 directions[4]) {
    BTContext ctx;
    ctx.world = &world;
    ctx.npc_index = npc_index;
    ctx.directions = directions;
    ctx.moved = false;

    // Condition: Check if ready to reproduce
    auto check_ready_reproduce = Condition([](BTContext& ctx) -> bool {
        return ready_to_reproduce(ctx);
    });

    // Action: Seek mate
    auto seek_mate_action = Action([](BTContext& ctx) {
        auto& transform = ctx.world->npcs.transform[ctx.npc_index];
        auto& restrictor = ctx.world->npcs.restrictor[ctx.npc_index];
#if ENABLE_NPC_PATHFINDING
        int2 mate_pos;
        size_t mate_index;
        if (find_mate(ctx, mate_pos, mate_index)) {
            int2 start = {(int)transform.x, (int)transform.y};
            int2 goal = mate_pos;
            auto path = astar(start, goal,
                [&](const int2& pos) { return restrictor->can_pass(pos); });
            if (path.size() > 1) {
                transform.x = path[1].x;
                transform.y = path[1].y;
                ctx.moved = true;
            } else {
                // Fallback: random movement if path not found
                int dirIdx = rand() % 4;
                int2 intDelta = ctx.directions[dirIdx];
                int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);
                if (restrictor->can_pass(newPos)) {
                    transform.x += intDelta.x;
                    transform.y += intDelta.y;
                    ctx.moved = true;
                }
            }
        }
#endif
    });

    // Sequence: Reproduction behavior
    auto reproduction_behavior = Sequence(check_ready_reproduce, seek_mate_action);

    // Condition: Check if prey is in range
    auto check_prey = Condition([](BTContext& ctx) -> bool {
        int2 prey_pos;
        bool is_hero;
        if (find_prey(ctx, prey_pos, is_hero)) {
            ctx.world->npcs.npcData[ctx.npc_index].targetPos = prey_pos;
            return true;
        }
        return false;
    });

    // Action: Hunt prey
    auto hunt_action = Action([](BTContext& ctx) {
        auto& transform = ctx.world->npcs.transform[ctx.npc_index];
        auto& restrictor = ctx.world->npcs.restrictor[ctx.npc_index];
        int2 prey_pos = ctx.world->npcs.npcData[ctx.npc_index].targetPos;
#if ENABLE_NPC_PATHFINDING
        int2 start = {(int)transform.x, (int)transform.y};
        int2 goal = prey_pos;

        auto path = astar(start, goal,
            [&](const int2& pos) { return restrictor->can_pass(pos); });

        if (path.size() > 1) {
            transform.x = path[1].x;
            transform.y = path[1].y;
            ctx.moved = true;
        }
#endif
    });

    // Sequence: Hunt behavior
    auto hunt_behavior = Sequence(check_prey, hunt_action);

    // Action: Idle (random movement)
    auto idle_behavior = Action([](BTContext& ctx) {
        if (!ctx.moved) {
            auto& transform = ctx.world->npcs.transform[ctx.npc_index];
            auto& restrictor = ctx.world->npcs.restrictor[ctx.npc_index];

            int dirIdx = rand() % 4;
            int2 intDelta = ctx.directions[dirIdx];
            int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);

            if (restrictor->can_pass(newPos)) {
                transform.x += intDelta.x;
                transform.y += intDelta.y;
            }
        }
    });

    // Main tree: Selector with hunt, reproduction and idle behaviors
    // Hunt has higher priority than reproduction to prevent predators from getting stuck
    auto predator_tree = Selector(
        hunt_behavior,
        reproduction_behavior,
        idle_behavior
    );

    return predator_tree.tick(ctx);
}