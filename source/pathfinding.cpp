#include "pathfinding.h"

#include <queue>
#include <unordered_map>
#include <functional>
#include <cmath>
#include <algorithm>

std::vector<int2> astar(const int2& start,
                        const int2& goal,
                        std::function<bool(const int2&)> can_pass,
                        std::function<bool(const int2&)> filter)
{
    if (start == goal)
        return {start};

    if (filter && !filter(goal))
        return {};

    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> open;
    std::unordered_map<int2, float, Int2Hash> gScore;
    std::unordered_map<int2, int2, Int2Hash> cameFrom;

    auto heuristic = [](const int2& a, const int2& b) -> float {
        return (float)(std::abs(a.x - b.x) + std::abs(a.y - b.y));
    };

    gScore[start] = 0.0f;
    open.push({start, 0.0f, heuristic(start, goal)});

    const int2 directions[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    while (!open.empty()) {
        AStarNode current = open.top();
        open.pop();

        if (current.pos == goal) {
            std::vector<int2> path;
            int2 pos = goal;
            while (!(pos == start)) {
                path.push_back(pos);
                pos = cameFrom[pos];
            }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (const auto& dir : directions) {
            int2 neighbor = {current.pos.x + dir.x, current.pos.y + dir.y};

            if (!can_pass(neighbor))
                continue;

            if (filter && !filter(neighbor))
                continue;

            float tentative_g = current.g + 1.0f;

            auto it = gScore.find(neighbor);
            if (it == gScore.end() || tentative_g < it->second) {
                cameFrom[neighbor] = current.pos;
                gScore[neighbor] = tentative_g;
                open.push({neighbor, tentative_g, heuristic(neighbor, goal)});
            }
        }
    }

    return {};
}
