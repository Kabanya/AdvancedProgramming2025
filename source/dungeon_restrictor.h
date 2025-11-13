#pragma once

#include "dungeon_generator.h"
#include "restrictor.h"

class DungeonRestrictor : public IRestrictor {
public:
    std::shared_ptr<Dungeon> dungeon;
    DungeonRestrictor(std::shared_ptr<Dungeon> dungeon)
        : dungeon(dungeon) {}

    bool can_pass(int2 coordinates) override {
        if (!dungeon)
            return false;
        const auto &grid = dungeon->getGrid();
        int x = coordinates.x;
        int y = coordinates.y;
        if (x < 0 || y < 0 || y >= (int)grid.size() || x >= (int)grid[0].size())
            return false;
        return grid[y][x] == Dungeon::FLOOR;
    }
};