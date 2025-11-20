
#include <cfloat>
#include <cstdlib>

#include "fsm.h"
#include "world.h"
#include "dungeon_restrictor.h"
#include "pathfinding.h"
#include "math2d.h"

#define ENABLE_NPC_PATHFINDING 1

void update_consumer_fsm(
    size_t npc_index,
    World& world,
    const int2 directions[4])
{
    auto& transform = world.npcs.transform[npc_index];
    auto& restrictor = world.npcs.restrictor[npc_index];
    auto& stamina = world.npcs.stamina[npc_index];
    auto& health = world.npcs.health[npc_index];
    auto& npcData = world.npcs.npcData[npc_index];

    auto currentState = std::get<ConsumerState>(npcData.state);

    bool predatorNearby = false;
    int2 nearestPredatorPos = {-1, -1};
    float minPredatorDist = FSMConfig::THREAT_RANGE;

    for (size_t p = 0; p < world.npcs.size(); ++p) {
        if (std::holds_alternative<NPCPredator>(world.npcs.npcType[p])) {
            float dist = std::abs(transform.x - world.npcs.transform[p].x) +
                        std::abs(transform.y - world.npcs.transform[p].y);
            if (dist < minPredatorDist) {
                predatorNearby = true;
                minPredatorDist = dist;
                nearestPredatorPos = {(int)world.npcs.transform[p].x, (int)world.npcs.transform[p].y};
            }
        }
    }

    if (should_flee(minPredatorDist) && predatorNearby) {
        if (currentState != ConsumerState::FLEEING) {
            npcData.state = ConsumerState::FLEEING;
            currentState = ConsumerState::FLEEING;
            npcData.targetPos = nearestPredatorPos;
        }
    } else if (ready_to_reproduce(health.current)) {
        if (currentState != ConsumerState::SEEKING_MATE) {
            npcData.state = ConsumerState::SEEKING_MATE;
            currentState = ConsumerState::SEEKING_MATE;
            npcData.targetPos = {-1, -1};
        }
    } else if (is_hungry(health.current, stamina.current)) {
        if (currentState != ConsumerState::SEEKING_FOOD) {
            npcData.state = ConsumerState::SEEKING_FOOD;
            currentState = ConsumerState::SEEKING_FOOD;
            npcData.targetPos = {-1, -1};
        }
    } else {
        if (currentState != ConsumerState::IDLE) {
            npcData.state = ConsumerState::IDLE;
            currentState = ConsumerState::IDLE;
            npcData.targetPos = {-1, -1};
        }
    }
    bool moved = false;

#if ENABLE_NPC_PATHFINDING
    if (currentState == ConsumerState::FLEEING) {
        // Move away from predator
        int2 currentPos = {(int)transform.x, (int)transform.y};
        int2 fleeDir = {
            currentPos.x - npcData.targetPos.x,
            currentPos.y - npcData.targetPos.y
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
                moved = true;
            } else {
                // Try perpendicular directions
                int2 altPos1 = {currentPos.x + fleeDir.y, currentPos.y};
                int2 altPos2 = {currentPos.x, currentPos.y + fleeDir.x};
                if (restrictor->can_pass(altPos1)) {
                    transform.x = altPos1.x;
                    transform.y = altPos1.y;
                    moved = true;
                } else if (restrictor->can_pass(altPos2)) {
                    transform.x = altPos2.x;
                    transform.y = altPos2.y;
                    moved = true;
                }
            }
        }
    } else if (currentState == ConsumerState::SEEKING_FOOD) {
        // Find nearest food
        size_t closestFood = (size_t)-1;
        float minDist = FLT_MAX;
        for (size_t f = 0; f < world.food.size(); ++f) {
            float dist = std::abs(transform.x - world.food.transform[f].x) +
                        std::abs(transform.y - world.food.transform[f].y);
            if (dist < minDist) {
                minDist = dist;
                closestFood = f;
            }
        }

        if (closestFood != (size_t)-1) {
            int2 start = {(int)transform.x, (int)transform.y};
            int2 goal = {(int)world.food.transform[closestFood].x, (int)world.food.transform[closestFood].y};

            auto path = astar(start, goal,
                [&](const int2& pos) { return restrictor->can_pass(pos); },
                [&](const int2& pos) {
                    // Avoid predators in pathfinding
                    for (size_t p = 0; p < world.npcs.size(); ++p) {
                        if (std::holds_alternative<NPCPredator>(world.npcs.npcType[p]) &&
                            int(world.npcs.transform[p].x) == pos.x &&
                            int(world.npcs.transform[p].y) == pos.y) {
                            return false;
                        }
                    }
                    return true;
                });

            if (path.size() > 1) {
                transform.x = path[1].x;
                transform.y = path[1].y;
                moved = true;
            }
        }
    } else if (currentState == ConsumerState::SEEKING_MATE) {
        // Find nearest mate of the same type
        size_t closestMate = (size_t)-1;
        float minDist = FSMConfig::MATE_SEARCH_RANGE;
        for (size_t m = 0; m < world.npcs.size(); ++m) {
            if (m == npc_index) continue;
            
            // Check if same type (consumer)
            if (!std::holds_alternative<NPCConsumer>(world.npcs.npcType[m])) continue;
            
            // Check if mate is ready to reproduce
            if (world.npcs.health[m].current <= FSMConfig::REPRODUCTION_THRESHOLD) continue;
            
            float dist = std::abs(transform.x - world.npcs.transform[m].x) +
                        std::abs(transform.y - world.npcs.transform[m].y);
            if (dist < minDist) {
                minDist = dist;
                closestMate = m;
            }
        }

        if (closestMate != (size_t)-1) {
            int2 start = {(int)transform.x, (int)transform.y};
            int2 goal = {(int)world.npcs.transform[closestMate].x, (int)world.npcs.transform[closestMate].y};

            auto path = astar(start, goal,
                [&](const int2& pos) { return restrictor->can_pass(pos); });

            if (path.size() > 1) {
                transform.x = path[1].x;
                transform.y = path[1].y;
                moved = true;
            } else {
                // Fallback: random movement if path not found
                int dirIdx = rand() % 4;
                int2 intDelta = directions[dirIdx];
                int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);
                if (restrictor->can_pass(newPos)) {
                    transform.x += intDelta.x;
                    transform.y += intDelta.y;
                    moved = true;
                }
            }
        }
    }
#endif // ENABLE_NPC_PATHFINDING

    // Idle or fallback: random movement
    if (!moved) {
        int dirIdx = rand() % 4;
        int2 intDelta = directions[dirIdx];
        int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);

        if (restrictor->can_pass(newPos)) {
            transform.x += intDelta.x;
            transform.y += intDelta.y;
        }
    }
}

void update_predator_fsm(
    size_t npc_index,
    World& world,
    const int2 directions[4])
{
    auto& transform = world.npcs.transform[npc_index];
    auto& restrictor = world.npcs.restrictor[npc_index];
    auto& npcData = world.npcs.npcData[npc_index];

    auto currentState = std::get<PredatorState>(npcData.state);

    size_t closestPrey = (size_t)-1;
    float minDist = FLT_MAX;
    bool isPreyHero = false;

    // Check for consumer NPCs
    for (size_t n = 0; n < world.npcs.size(); ++n) {
        if (n == npc_index) continue;
        if (std::holds_alternative<NPCConsumer>(world.npcs.npcType[n])) {
            float dist = std::abs(transform.x - world.npcs.transform[n].x) +
                        std::abs(transform.y - world.npcs.transform[n].y);
            if (dist < minDist) {
                minDist = dist;
                closestPrey = n;
                isPreyHero = false;
            }
        }
    }

    // Check for hero
    for (size_t h = 0; h < world.hero.size(); ++h) {
        float dist = std::abs(transform.x - world.hero.transform[h].x) +
                    std::abs(transform.y - world.hero.transform[h].y);
        if (dist < minDist) {
            minDist = dist;
            closestPrey = h;
            isPreyHero = true;
        }
    }

    // State transitions
    // Hunt has higher priority than reproduction to prevent predators from getting stuck
    if (closestPrey != (size_t)-1 && prey_in_range(minDist)) {
        if (currentState != PredatorState::HUNTING) {
            npcData.state = PredatorState::HUNTING;
            currentState = PredatorState::HUNTING;
        }
    } else if (ready_to_reproduce(world.npcs.health[npc_index].current)) {
        if (currentState != PredatorState::SEEKING_MATE) {
            npcData.state = PredatorState::SEEKING_MATE;
            currentState = PredatorState::SEEKING_MATE;
            npcData.targetPos = {-1, -1};
        }
    } else {
        if (currentState != PredatorState::IDLE) {
            npcData.state = PredatorState::IDLE;
            currentState = PredatorState::IDLE;
            npcData.targetPos = {-1, -1};
        }
    }

    bool moved = false;

#if ENABLE_NPC_PATHFINDING
    if (currentState == PredatorState::HUNTING && closestPrey != (size_t)-1) {
        int2 start = {(int)transform.x, (int)transform.y};
        int2 goal;

        if (isPreyHero) {
            goal = {(int)world.hero.transform[closestPrey].x, (int)world.hero.transform[closestPrey].y};
        } else {
            goal = {(int)world.npcs.transform[closestPrey].x, (int)world.npcs.transform[closestPrey].y};
        }

        auto path = astar(start, goal,
            [&](const int2& pos) { return restrictor->can_pass(pos); });

        if (path.size() > 1) {
            transform.x = path[1].x;
            transform.y = path[1].y;
            moved = true;
        }
    } else if (currentState == PredatorState::SEEKING_MATE) {
        // Find nearest mate of the same type
        size_t closestMate = (size_t)-1;
        float minDist = FSMConfig::MATE_SEARCH_RANGE;
        for (size_t m = 0; m < world.npcs.size(); ++m) {
            if (m == npc_index) continue;
            
            // Check if same type (predator)
            if (!std::holds_alternative<NPCPredator>(world.npcs.npcType[m])) continue;
            
            // Check if mate is ready to reproduce
            if (world.npcs.health[m].current <= FSMConfig::REPRODUCTION_THRESHOLD) continue;
            
            float dist = std::abs(transform.x - world.npcs.transform[m].x) +
                        std::abs(transform.y - world.npcs.transform[m].y);
            if (dist < minDist) {
                minDist = dist;
                closestMate = m;
            }
        }

        if (closestMate != (size_t)-1) {
            int2 start = {(int)transform.x, (int)transform.y};
            int2 goal = {(int)world.npcs.transform[closestMate].x, (int)world.npcs.transform[closestMate].y};

            auto path = astar(start, goal,
                [&](const int2& pos) { return restrictor->can_pass(pos); });

            if (path.size() > 1) {
                transform.x = path[1].x;
                transform.y = path[1].y;
                moved = true;
            } else {
                // Fallback: random movement if path not found
                int dirIdx = rand() % 4;
                int2 intDelta = directions[dirIdx];
                int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);
                if (restrictor->can_pass(newPos)) {
                    transform.x += intDelta.x;
                    transform.y += intDelta.y;
                    moved = true;
                }
            }
        }
    }
#endif // ENABLE_NPC_PATHFINDING

    // Idle or fallback: random movement
    if (!moved) {
        int dirIdx = rand() % 4;
        int2 intDelta = directions[dirIdx];
        int2 newPos = int2((int)transform.x + intDelta.x, (int)transform.y + intDelta.y);

        if (restrictor->can_pass(newPos)) {
            transform.x += intDelta.x;
            transform.y += intDelta.y;
        }
    }
}