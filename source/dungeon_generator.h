#pragma once
#include <vector>
#include <random>
#include <algorithm>
#include "math2d.h"

struct Room {
    int x, y, w, h;
    int centerX() const { return x + w / 2; }
    int centerY() const { return y + h / 2; }
};

class Dungeon {
public:
    enum Tile {
        WALL,
        FLOOR
    };
    Dungeon(int width, int height, int roomAttempts = 50)
        : W(width), H(height), grid(height, std::vector<Tile>(width, WALL)) {
        generate(roomAttempts);
    }

    const std::vector<std::vector<Tile>> &getGrid() const {
        return grid;
    }

    // Возвращает случайную позицию напольного тайла
    // Не эффективно для больших карт, но сойдет для примера
    int2 getRandomFloorPosition() {
        std::uniform_int_distribution<size_t> dist(0, floorCount - 1);
        size_t target = dist(rng);
        size_t count = 0;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (grid[y][x] == FLOOR) {
                    if (count == target) {
                        return int2{x, y};
                    }
                    count++;
                }
            }
        }
        return int2{-1, -1}; // Не должно случиться
    }

private:
    int W, H;
    int floorCount = 0;
    std::vector<std::vector<Tile>> grid;
    std::vector<Room> rooms;
    std::mt19937 rng{ std::random_device{}() };

    void generate(int roomAttempts) {
        std::uniform_int_distribution<int> rw(4, 10);
        std::uniform_int_distribution<int> rh(4, 8);
        std::uniform_int_distribution<int> rx(1, W - 12);
        std::uniform_int_distribution<int> ry(1, H - 10);

        // Ставим комнаты
        for (int i = 0; i < roomAttempts; i++) {
            Room r{ rx(rng), ry(rng), rw(rng), rh(rng) };
            if (placeRoom(r)) rooms.push_back(r);
        }

        // Соединяем комнаты коридорами
        for (size_t i = 1; i < rooms.size(); i++) {
            auto& a = rooms[i - 1];
            auto& b = rooms[i];
            connectRooms(a, b);
        }
        // Подсчитываем количество напольных тайлов (для getRandomFloorPosition)
        floorCount = 0;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (grid[y][x] == FLOOR) {
                    floorCount++;
                }
            }
        }
    }

    bool placeRoom(const Room& r) {
        // Проверка выхода за границы
        if (r.x < 1 || r.y < 1 || r.x + r.w >= W - 1 || r.y + r.h >= H - 1)
            return false;

        // Проверка пересечения
        for (int y = r.y - 1; y < r.y + r.h + 1; y++) {
            for (int x = r.x - 1; x < r.x + r.w + 1; x++) {
                if (grid[y][x] == FLOOR) return false;
            }
        }

        // Рисуем комнату
        for (int y = r.y; y < r.y + r.h; y++) {
            for (int x = r.x; x < r.x + r.w; x++) {
                grid[y][x] = FLOOR;
            }
        }
        return true;
    }

    void connectRooms(const Room& a, const Room& b) {
        int x1 = a.centerX(), y1 = a.centerY();
        int x2 = b.centerX(), y2 = b.centerY();

        // Простейший L-образный коридор
        if (std::uniform_int_distribution<int>(0,1)(rng)) {
            carveHorizontal(x1, x2, y1);
            carveVertical(y1, y2, x2);
        } else {
            carveVertical(y1, y2, x1);
            carveHorizontal(x1, x2, y2);
        }
    }

    void carveHorizontal(int x1, int x2, int y) {
        if (x2 < x1) std::swap(x1, x2);
        for (int x = x1; x <= x2; x++) grid[y][x] = FLOOR;
    }

    void carveVertical(int y1, int y2, int x) {
        if (y2 < y1) std::swap(y1, y2);
        for (int y = y1; y <= y2; y++) grid[y][x] = FLOOR;
    }
};
