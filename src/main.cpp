#include "Engine/collision.h"
#include "Engine/core.h"
#include "Engine/engine.h"
#include "Engine/input.h"
#include "Engine/scaling.h"
#include <SDL3/SDL_scancode.h>

// basic demo of engine features.

Engine::Entity* player;
Engine::Entity* platform;

void update(float dt);
void switchScalingMode();

// annoying thing about not having key up / down events
// you have to manually track them
bool modeKeyPressed = false;

int main() {
    // init, terminate if unsuccessful
    if (!Engine::init("Game Engine Demo")) return 1;

    Engine::setBackgroundColor(30, 30, 30);

    // set up key mapping
    Engine::Input::map("left", SDL_SCANCODE_A);
    Engine::Input::map("left", SDL_SCANCODE_LEFT);

    Engine::Input::map("right", SDL_SCANCODE_D);
    Engine::Input::map("right", SDL_SCANCODE_RIGHT);

    Engine::Input::map("jump", SDL_SCANCODE_W);
    Engine::Input::map("jump", SDL_SCANCODE_UP);
    Engine::Input::map("jump", SDL_SCANCODE_SPACE);

    Engine::Input::map("mode", SDL_SCANCODE_M);

    // create the player and enable physics on it
    player = new Engine::Entity("media/square.png");
    player->setGravity(true);
    player->setFriction(500, 0); // friction of 500 px/sec^2 in the x direction, no friction in y direction.
    player->setMaxSpeed(1000, 0); // max speed of 1000px/sec in the x direction, no limit the y direction.

    // create the platform entity
    platform = new Engine::Entity("media/platform.png");
    platform->setPos(1000 - platform->getWidth() / 2, 500);

    return Engine::main(update);
}

void update(float dt) {
    // for jumping
    bool onGround = false;

    // add a floor to our scene
    if (player->getPosY() > 800) {
        if (player->getVelocity().y > 0)
            player->setVelocityY(0);
        player->setPosY(800);
        onGround = true;
    }

    // check for collisions with the platform
    // collisions are complicated!
    switch (Engine::Collision::checkEdge(player, platform)) {
        case Engine::Collision::NO_COLLISION:
            break;
        case Engine::Collision::LEFT:
            // the left edge of the player is touching the platform.
            if (player->getVelocity().x < 0)
                player->setVelocityX(0);
            player->setPosX(platform->getPosX() + platform->getWidth());
            break;
        case Engine::Collision::RIGHT:
            // the right edge of the player is touching the platform
            if (player->getVelocity().x > 0)
                player->setVelocityX(0);
            player->setPosX(platform->getPosX() - player->getWidth());
            break;
        case Engine::Collision::TOP:
            // the top edge of the player is touching the platform
            if (player->getVelocity().y < 0)
                player->setVelocityY(0);
            player->setPosY(platform->getPosY() + platform->getHeight());
            break;
        case Engine::Collision::BOTTOM:
            // the bottom edge of the player is touching the platform
            if (player->getVelocity().y > 0)
                player->setVelocityY(0);
            player->setPosY(platform->getPosY() - player->getHeight());
            onGround = true;
            break;
    }

    // keyboard input
    // apply a force to move the player.
    // forces that are applied over time should be scaled by time (dt).
    if (Engine::Input::keyPressed("left"))
        player->applyForce(-2000.0f * dt, 0.0f);

    if (Engine::Input::keyPressed("right"))
        player->applyForce(2000.0f * dt, 0.0f);

    if (Engine::Input::keyPressed("jump") && onGround)
        player->applyForce(0.0f, -1500.0f);

    // handle scaling mode changes
    if (Engine::Input::keyPressed("mode")) {
        bool keyDown = !modeKeyPressed;
        if (keyDown) {
            switchScalingMode();
        }

        modeKeyPressed = true;
    } else {
        modeKeyPressed = false;
    }
}

void switchScalingMode() {
    // cycle through the display modes. definitely an abuse of the implentation, not recommended!
    Engine::Scaling::setMode(Engine::Scaling::getMode() + 1);
}
