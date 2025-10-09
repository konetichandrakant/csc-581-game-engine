#include "entity.h"
#include "core.h"
#include "scaling.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

namespace Engine {

    Entity::Entity(SDL_Texture* texture) {
        this->texture = texture;

        registerEntity(this);
    };

    Entity::Entity(const char* filePath) {
        if (!renderer)
            SDL_Log("Renderer is invalid! make sure to call Engine::init() before creating entities.");
        texture = IMG_LoadTexture(renderer, filePath);
        if (!texture)
            SDL_Log("failed to load texture: %s", SDL_GetError());

        registerEntity(this);
    };

    Entity::~Entity() {
        unregisterEntity(this);
    };

    void Entity::update(float dt) {
        // intentionally left blank
    }

    void Entity::draw() {
        // TODO: should we draw the sprite centered at (x,y) instead of (x, y) being at the top right?

        // Source (src) Rectangle is capturing the image from the spritesheet
        // for now, the whole image
        SDL_FRect srcRect = { /* x(position), y(position), width, height */
            0.0f,
            0.0f,
            (float)texture->w,
            (float)texture->h
        };

        // Destination (dst) Rectangle is drawing the image on the window
        SDL_FRect dstRect = Scaling::apply(getBoundingBox());

        SDL_RenderTexture(renderer, texture, &srcRect, &dstRect);
    };

    void Entity::setPos(float x, float y) {
        pos.x = x;
        pos.y = y;
    };

    void Entity::setPos(Vec2& newPos) {
        pos.x = newPos.x;
        pos.y = newPos.y;
    };

    void Entity::translate(float x, float y) {
        pos.x += x;
        pos.y += y;
    };

    void Entity::translate(Vec2& delta) {
        pos.x += delta.x;
        pos.y += delta.y;
    };

    SDL_FRect Entity::getBoundingBox() {
        return { /* x(position), y(position), width, height */
            pos.x,
            pos.y,
            getWidth(),
            getHeight()
        };
    }

    float Entity::getWidth() {
        return (float)texture->w;
    }

    float Entity::getHeight() {
        return (float)texture->h;
    }
}
