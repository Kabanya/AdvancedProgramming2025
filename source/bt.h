#pragma once

#include "math2d.h"
#include "world.h"
#include <cfloat>
#include <utility>

enum class BTStatus {
    SUCCESS,
    FAILURE,
    RUNNING
};


struct BTContext {
    World* world;
    size_t npc_index;
    const int2* directions;
    bool moved;
};


namespace BTConfig {
    constexpr float THREAT_RANGE = 5.0f;
    constexpr float HUNGER_THRESHOLD = 50.0f;
    constexpr float HUNT_RANGE = 8.0f;
}

template<typename Derived>
struct BTNodeBase {
    BTStatus tick(BTContext& ctx) {
        return static_cast<Derived*>(this)->execute(ctx);
    }
};

template<typename... Children>
struct Sequence : BTNodeBase<Sequence<Children...>> {
    std::tuple<Children...> children;

    explicit Sequence(Children... args) : children(std::make_tuple(std::move(args)...)) {}
    template<typename... Args>
    explicit Sequence(Args&&... args) : children(std::forward<Args>(args)...) {}

    BTStatus execute(BTContext& ctx) {
        return execute_impl(ctx, std::index_sequence_for<Children...>{});
    }

private:
    template<size_t... Is>
    BTStatus execute_impl(BTContext& ctx, std::index_sequence<Is...>) {
        BTStatus result = BTStatus::SUCCESS;
        // Fold expression to execute children in order
        ((result = execute_child<Is>(ctx, result)) , ...);
        return result;
    }

    template<size_t I>
    BTStatus execute_child(BTContext& ctx, BTStatus prev) {
        if (prev != BTStatus::SUCCESS) return prev;
        return std::get<I>(children).tick(ctx);
    }
};

template<typename... Children>
Sequence(Children...) -> Sequence<Children...>;

template<typename... Children>
struct Selector : BTNodeBase<Selector<Children...>> {
    std::tuple<Children...> children;

    explicit Selector(Children... args) : children(std::make_tuple(std::move(args)...)) {}
    template<typename... Args>
    explicit Selector(Args&&... args) : children(std::forward<Args>(args)...) {}

    BTStatus execute(BTContext& ctx) {
        return execute_impl(ctx, std::index_sequence_for<Children...>{});
    }

private:
    template<size_t... Is>
    BTStatus execute_impl(BTContext& ctx, std::index_sequence<Is...>) {
        BTStatus result = BTStatus::FAILURE;
        // Fold expression to execute children until success
        ((result = execute_child<Is>(ctx, result)) , ...);
        return result;
    }

    template<size_t I>
    BTStatus execute_child(BTContext& ctx, BTStatus prev) {
        if (prev == BTStatus::SUCCESS) return prev;
        BTStatus status = std::get<I>(children).tick(ctx);
        return (status == BTStatus::SUCCESS || status == BTStatus::RUNNING) ? status : prev;
    }
};

// Deduction guide for Selector
template<typename... Children>
Selector(Children...) -> Selector<Children...>;

template<typename Func>
struct Action : BTNodeBase<Action<Func>> {
    Func func;

    explicit Action(Func f) : func(std::move(f)) {}

    BTStatus execute(BTContext& ctx) {
        func(ctx);
        return BTStatus::SUCCESS;
    }
};

// Deduction guide for Action
template<typename Func>
Action(Func) -> Action<Func>;

template<typename Func>
struct Condition : BTNodeBase<Condition<Func>> {
    Func func;

    explicit Condition(Func f) : func(std::move(f)) {}

    BTStatus execute(BTContext& ctx) {
        return func(ctx) ? BTStatus::SUCCESS : BTStatus::FAILURE;
    }
};

// Deduction guide for Condition
template<typename Func>
Condition(Func) -> Condition<Func>;

template<typename Child>
struct Inverter : BTNodeBase<Inverter<Child>> {
    Child child;

    explicit Inverter(Child c) : child(std::move(c)) {}

    BTStatus execute(BTContext& ctx) {
        BTStatus status = child.tick(ctx);
        if (status == BTStatus::SUCCESS) return BTStatus::FAILURE;
        if (status == BTStatus::FAILURE) return BTStatus::SUCCESS;
        return status;
    }
};

template<typename Child>
Inverter(Child) -> Inverter<Child>;


inline bool is_predator_nearby(BTContext& ctx, int2& predator_pos) {
    auto& transform = ctx.world->npcs.transform[ctx.npc_index];
    float minDist = BTConfig::THREAT_RANGE;
    bool found = false;
    for (size_t p = 0; p < ctx.world->npcs.size(); ++p) {
        if (std::holds_alternative<NPCPredator>(ctx.world->npcs.npcType[p])) {
            float dist = std::abs(transform.x - ctx.world->npcs.transform[p].x) +
                        std::abs(transform.y - ctx.world->npcs.transform[p].y);
            if (dist < minDist) {
                minDist = dist;
                predator_pos = {(int)ctx.world->npcs.transform[p].x, (int)ctx.world->npcs.transform[p].y};
                found = true;
            }
        }
    }
    return found;
}

inline bool is_npc_hungry(BTContext& ctx) {
    auto& health = ctx.world->npcs.health[ctx.npc_index];
    auto& stamina = ctx.world->npcs.stamina[ctx.npc_index];
    return health.current < BTConfig::HUNGER_THRESHOLD || stamina.current < BTConfig::HUNGER_THRESHOLD;
}

inline bool find_prey(BTContext& ctx, int2& prey_pos, bool& is_hero) {
    auto& transform = ctx.world->npcs.transform[ctx.npc_index];
    float minDist = FLT_MAX;
    bool found = false;

    for (size_t n = 0; n < ctx.world->npcs.size(); ++n) {
        if (n == ctx.npc_index) continue;
        if (std::holds_alternative<NPCConsumer>(ctx.world->npcs.npcType[n])) {
            float dist = std::abs(transform.x - ctx.world->npcs.transform[n].x) +
                        std::abs(transform.y - ctx.world->npcs.transform[n].y);
            if (dist < minDist) {
                minDist = dist;
                prey_pos = {(int)ctx.world->npcs.transform[n].x, (int)ctx.world->npcs.transform[n].y};
                is_hero = false;
                found = true;
            }
        }
    }

    // Check hero
    for (size_t h = 0; h < ctx.world->hero.size(); ++h) {
        float dist = std::abs(transform.x - ctx.world->hero.transform[h].x) +
                    std::abs(transform.y - ctx.world->hero.transform[h].y);
        if (dist < minDist) {
            minDist = dist;
            prey_pos = {(int)ctx.world->hero.transform[h].x, (int)ctx.world->hero.transform[h].y};
            is_hero = true;
            found = true;
        }
    }

    return found && minDist < BTConfig::HUNT_RANGE;
}
BTStatus update_consumer_bt(size_t npc_index, World& world, const int2 directions[4]);
BTStatus update_predator_bt(size_t npc_index, World& world, const int2 directions[4]);