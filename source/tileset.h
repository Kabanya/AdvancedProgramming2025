#pragma once
#include "sprite.h"
#include <string>
#include <map>

struct TileSet
{
    TexturePtr tileset;
    TileSet(TexturePtr tileset)
    {
        this->tileset = tileset;
        add_tile("floor1", 4, 0);
        add_tile("floor2", 4, 1);
        add_tile("wall", 3, 4);
        add_tile("knight", 8, 1);
        add_tile("peasant", 7, 1);
        add_tile("ghost", 10, 1);
        add_tile("health_small", 10, 7);
        add_tile("health_large", 9, 7);
        add_tile("stamina_small", 10, 8);
        add_tile("stamina_large", 9, 8);
    }
    std::map<std::string, Sprite> tiles;
    void add_tile(const std::string& name, int i, int j)
    {
        const int tileSize = 16;
        tiles[name] = Sprite(tileset, SDL_FRect{float(j * (tileSize + 1)), float(i * (tileSize + 1)), float(tileSize), float(tileSize)});
    }
    Sprite get_tile(const std::string& name)
    {
        return tiles.at(name);
    }
};