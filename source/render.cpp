#include "world.h"
#include "camera2d.h"
#include "sprite.h"
#include "health.h"
#include "stamina.h"
#include <SDL3/SDL_render.h>

void render_world(SDL_Window* window, SDL_Renderer* renderer, World& world)
{
    int screenW, screenH;
    SDL_GetWindowSize(window, &screenW, &screenH);

    // Get camera (assume first camera if exists)
    if (world.camera.size() == 0)
        return;

    const auto& camera2d = world.camera.camera[0];
    const auto& camera_transform = world.camera.transform[0];

    // Draw background sprites (tiles)
    for (size_t i = 0; i < world.tiles.size(); ++i) {
        const auto& sprite = world.tiles.sprite[i];
        const auto& transform = world.tiles.transform[i];

        SDL_FRect dst = to_camera_space(transform, camera_transform, camera2d);
        dst.x += screenW / 2.f;
        dst.y += screenH / 2.f;
        DrawSprite(renderer, sprite, dst);
    }

    // Draw food (background layer)
    for (size_t i = 0; i < world.food.size(); ++i) {
        const auto& sprite = world.food.sprite[i];
        const auto& transform = world.food.transform[i];

        SDL_FRect dst = to_camera_space(transform, camera_transform, camera2d);
        dst.x += screenW / 2.f;
        dst.y += screenH / 2.f;
        DrawSprite(renderer, sprite, dst);
    }

    // Draw foreground sprites - NPCs
    for (size_t i = 0; i < world.npcs.size(); ++i) {
        const auto& sprite = world.npcs.sprite[i];
        const auto& transform = world.npcs.transform[i];

        SDL_FRect dst = to_camera_space(transform, camera_transform, camera2d);
        dst.x += screenW / 2.f;
        dst.y += screenH / 2.f;
        DrawSprite(renderer, sprite, dst);
    }

    // Draw foreground sprites - Hero
    for (size_t i = 0; i < world.hero.size(); ++i) {
        const auto& sprite = world.hero.sprite[i];
        const auto& transform = world.hero.transform[i];

        SDL_FRect dst = to_camera_space(transform, camera_transform, camera2d);
        dst.x += screenW / 2.f;
        dst.y += screenH / 2.f;
        DrawSprite(renderer, sprite, dst);
    }

    // Draw bars without textures and without OOP
    float grayColor[4] = {0.2f, 0.2f, 0.2f, 1.f};
    float healthColor[4] = {0.91f, 0.27f, 0.22f, 1.f};
    float staminaColor[4] = {0.f, 0.6f, 0.86f, 1.f};

    std::vector<SDL_FRect> backBars;
    std::vector<SDL_FRect> healthBars;
    std::vector<SDL_FRect> staminaBars;

    // Draw bars for Hero
    for (size_t i = 0; i < world.hero.size(); ++i) {
        const auto& transform = world.hero.transform[i];
        const auto& health = world.hero.health[i];
        const auto& stamina = world.hero.stamina[i];

        // Health bar
        {
            Transform2D barTransform = transform;
            barTransform.sizeX *= 0.1f;
            SDL_FRect dst = to_camera_space(barTransform, camera_transform, camera2d);
            dst.x += screenW / 2.f;
            dst.y += screenH / 2.f;
            backBars.push_back(dst);
            const float value = float(health.current) / float(health.max);
            dst.y += (1.f - value) * dst.h;
            dst.h *= value;
            healthBars.push_back(dst);
        }

        // Stamina bar
        {
            Transform2D barTransform = transform;
            barTransform.x += barTransform.sizeX * 0.9f;
            barTransform.sizeX *= 0.1f;
            SDL_FRect dst = to_camera_space(barTransform, camera_transform, camera2d);
            dst.x += screenW / 2.f;
            dst.y += screenH / 2.f;
            backBars.push_back(dst);
            const float value = float(stamina.current) / float(stamina.max);
            dst.y += (1.f - value) * dst.h;
            dst.h *= value;
            staminaBars.push_back(dst);
        }
    }

    // Draw bars for NPCs
    for (size_t i = 0; i < world.npcs.size(); ++i) {
        const auto& transform = world.npcs.transform[i];
        const auto& health = world.npcs.health[i];
        const auto& stamina = world.npcs.stamina[i];

        // Health bar
        {
            Transform2D barTransform = transform;
            barTransform.sizeX *= 0.1f;
            SDL_FRect dst = to_camera_space(barTransform, camera_transform, camera2d);
            dst.x += screenW / 2.f;
            dst.y += screenH / 2.f;
            backBars.push_back(dst);
            const float value = float(health.current) / float(health.max);
            dst.y += (1.f - value) * dst.h;
            dst.h *= value;
            healthBars.push_back(dst);
        }

        // Stamina bar
        {
            Transform2D barTransform = transform;
            barTransform.x += barTransform.sizeX * 0.9f;
            barTransform.sizeX *= 0.1f;
            SDL_FRect dst = to_camera_space(barTransform, camera_transform, camera2d);
            dst.x += screenW / 2.f;
            dst.y += screenH / 2.f;
            backBars.push_back(dst);
            const float value = float(stamina.current) / float(stamina.max);
            dst.y += (1.f - value) * dst.h;
            dst.h *= value;
            staminaBars.push_back(dst);
        }
    }

    SDL_SetRenderDrawColorFloat(renderer, grayColor[0], grayColor[1], grayColor[2], grayColor[3]);
    SDL_RenderFillRects(renderer, backBars.data(), int(backBars.size()));
    SDL_SetRenderDrawColorFloat(renderer, healthColor[0], healthColor[1], healthColor[2], healthColor[3]);
    SDL_RenderFillRects(renderer, healthBars.data(), int(healthBars.size()));
    SDL_SetRenderDrawColorFloat(renderer, staminaColor[0], staminaColor[1], staminaColor[2], staminaColor[3]);
    SDL_RenderFillRects(renderer, staminaBars.data(), int(staminaBars.size()));
}