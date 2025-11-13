#pragma once
#include "image.h"

struct Sprite {
    TexturePtr texture;
    SDL_FRect src;
    Sprite() : texture(nullptr), src{0, 0, 0, 0} {}
    Sprite(TexturePtr texture, const SDL_FRect& src)
        : texture(texture), src(src) {}
};

inline void DrawSprite(SDL_Renderer* renderer, const Sprite& sprite, const SDL_FRect &dst)
{
    if (!sprite.texture) return;

    SDL_RenderTexture(renderer, sprite.texture.get(), &sprite.src, &dst);
}