#include <SDL3/SDL.h>
#include <iostream>
#include <atomic>
#include "optick.h"
#include "world.h"
// #include <stacktrace>

#include "config.h"

void init_world(SDL_Renderer* renderer, World& world);
void render_world(SDL_Window* window, SDL_Renderer* renderer, World& world);

// RCU-like approach: atomic pointer to current world
std::atomic<World*> g_currentWorld{nullptr};

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL could not initialize! SDL_Error: "
                  << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Advanced Programming Course(Zharinov/Egor)",
        1600, 1200,
        SDL_WINDOW_RESIZABLE // вместо SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: "
                  << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: "
                  << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    {
        // Initialize first world
        World* world = new World();
        {
            OPTICK_EVENT("world.init");
            init_world(renderer, *world);
        }
        g_currentWorld.store(world, std::memory_order_release);

        bool quit = false;
        SDL_Event e;
        Uint64 lastTicks = SDL_GetTicks();

        while (!quit) {

	        OPTICK_FRAME("MainThread");
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) {
                    quit = true;
                }
                // Handle R key for world regeneration
                if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R) {
                    OPTICK_EVENT("world.regenerate");
                    // Create new world
                    World* newWorld = new World();
                    init_world(renderer, *newWorld);
                    // Atomically swap worlds (RCU write)
                    World* oldWorld = g_currentWorld.exchange(newWorld, std::memory_order_acq_rel);
                    // Defer deletion - in real RCU we'd wait for grace period
                    // For simplicity, delete immediately (readers use local pointer)
                    delete oldWorld;
                }
            }
            Uint64 now = SDL_GetTicks();
            float deltaTime = (now - lastTicks) / 1000.0f;
            lastTicks = now;
            // RCU read: get current world pointer
            World* currentWorld = g_currentWorld.load(std::memory_order_acquire);

#if USE_SPINLOCK
            {
                OPTICK_EVENT("spinlock.world.update");
                currentWorld->update_spnlck(deltaTime);
            }
#endif
#if USE_MUTEX
            {
                OPTICK_EVENT("mutex.world.update");
                currentWorld->update_ts(deltaTime);
            }
#endif
#if USE_THREADS
            {
                OPTICK_EVENT("threads.world.update");
                currentWorld->world_update_threaded(deltaTime);
            }
#endif
#if USE_THREAD_POOL
            {
                OPTICK_EVENT("thread_pool.world.update");
                currentWorld->world_update_thread_pool(deltaTime);
            }
#endif
#if !USE_MUTEX && !USE_THREADS && !USE_THREAD_POOL && !USE_SPINLOCK
            {
                OPTICK_EVENT("world.update");
                currentWorld->update(deltaTime);
            }
#endif
            // Теперь сразу цвет внутри Clear
            SDL_SetRenderDrawColor(renderer, 50, 50, 150, 255);
            SDL_RenderClear(renderer);
            {
                OPTICK_EVENT("world.render");
                // Отрисовка всех игровых объектов
                render_world(window, renderer, *currentWorld);
            }

            SDL_RenderPresent(renderer);
        }
        // Cleanup
        World* finalWorld = g_currentWorld.load(std::memory_order_acquire);
        delete finalWorld;
        g_currentWorld.store(nullptr, std::memory_order_release);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}