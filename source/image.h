#pragma once

#include <memory>
#include <SDL3/SDL_render.h>

using TexturePtr = std::shared_ptr<SDL_Texture>;

TexturePtr LoadTextureFromFile(const char* filename, SDL_Renderer* renderer);
