#pragma once

#include <variant>


enum class ConsumerState {
    IDLE,
    SEEKING_FOOD,
    FLEEING,
    SEEKING_MATE
};

enum class PredatorState {
    IDLE,
    HUNTING,
    SEEKING_MATE
};

using NPCState = std::variant<ConsumerState, PredatorState>;

namespace FSMConfig {
    constexpr float THREAT_RANGE = 5.0f;
    constexpr float HUNGER_THRESHOLD = 50.0f;
    constexpr float HUNT_RANGE = 8.0f;
    constexpr int REPRODUCTION_THRESHOLD = 90;
    constexpr float MATE_SEARCH_RANGE = 10.0f;
}

template<typename NPCType>
inline NPCState get_initial_state(const NPCType& type) {
    if constexpr (std::is_same_v<NPCType, struct NPCConsumer>) {
        return ConsumerState::IDLE;
    } else {
        return PredatorState::IDLE;
    }
}

inline bool should_flee(float predator_distance) {
    return predator_distance < FSMConfig::THREAT_RANGE;
}

inline bool is_hungry(int health, int stamina) {
    return health < FSMConfig::HUNGER_THRESHOLD || stamina < FSMConfig::HUNGER_THRESHOLD;
}

inline bool prey_in_range(float prey_distance) {
    return prey_distance < FSMConfig::HUNT_RANGE;
}

inline bool ready_to_reproduce(int health) {
    return health > FSMConfig::REPRODUCTION_THRESHOLD;
}