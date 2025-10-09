#pragma once
#include "vec2.h"
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <string>

namespace Engine {
    class Entity {
        private:
            /*
             * position of the entity
             */
            Vec2 pos = {0, 0};

            /*
             * velocity of the entity
             */
            Vec2 vel = {0, 0};

            /*
             * entity's texture for drawing
             */
            SDL_Texture* texture;

            /*
             * whether the entity is affected by gravity.
             */
            bool gravity = false;

            /*
             * whether the entity has collisions:
             *
             * if false, (Entity::Collision::all()) will ignore this entity.
             *
             */
            bool collisions = true;

            /*
             * whether the entity has physics.
             */
            bool physics = true;

            /*
             * friction applied to the object.
             */
             Vec2 friction = {0, 0};
            /*
             * max speed applied to object. Note: (0, 0) is a special case where no
             * maximum speed is applied to the object.
             */
            Vec2 maxSpeed = {0, 0};

            std::string type = "Entity";


        public:

            /*
             * constructors. can pass the texture itself, or a file path to get the texture from.
             * position defaults to (0, 0).
             */
            Entity(SDL_Texture* texture);
            Entity(const char* filePath);
            ~Entity();

            /*
             * update function gets called automatically once per frame by the engine main loop.
             *
             * by default, it does nothing, but the idea is you could extend this class and override
             * this method to implement custom behavior.
             */
            virtual void update(float time);

            /*
             * draw the entity to the screen.
             */
            void draw();

            /*
             * set the position of the entity
             */
            void setPos(float x, float y);
            void setPos(Vec2& newPosition);
            void setPosX(float x) {pos.x = x;}
            void setPosY(float y) {pos.y = y;}

            /*
             * get the position of the entity
             */
             Vec2& getPos() {return pos;}
             float getPosX() {return pos.x;}
             float getPosY() {return pos.y;}

            /*
             * translate (move) the entity by the given amount.
             */
            void translate(float x, float y);
            void translate(Vec2& delta);

            /*
             * check whether or not the entity is affected by gravity.
             */
            bool hasGravity() {return gravity;}

            /*
             * enable or disable gravity on this entity.
             */
            void setGravity(bool g) {gravity = g;}

            /*
             * check whether or not the entity is collidable.
             */
            bool hasCollisions() {return collisions;}

            /*
             * enable or disable collisions on this entity.
             */
            void setCollisions(bool c) {collisions = c;}

            /*
             * check whether or not the entity is affected by physics.
             */
            bool hasPhysics() {return physics;}

            /*
             * enable or disable physics on this entity.
             */
            void setPhysics(bool p) {physics = p;}

            /*
             * set the strength of friction in the x and y directions.
             */
            void setFriction(float x, float y) {friction.x = x; friction.y = y;}
            void setFriction(Vec2& fric) {friction.x = fric.x; friction.y = fric.y;}

            Vec2& getFriction() {return friction;}

            /*
             * set the max speed in the x and y directions.
             */
            void setMaxSpeed(float x, float y) {maxSpeed.x = x; maxSpeed.y = y;}
            void setMaxSpeed(Vec2& max) {maxSpeed.x = max.x; maxSpeed.y = max.y;}

            Vec2& getMaxSpeed() {return maxSpeed;}

            /*
             * set the velocity of the entity
             */
             void setVelocity(Vec2& velocity) {vel.x = velocity.x; vel.y = velocity.y;}
             void setVelocity(float x, float y) {vel.x = x; vel.y = y;}
             void setVelocityX(float x) {vel.x = x;}
             void setVelocityY(float y) {vel.y = y;}

             Vec2& getVelocity() {return vel;}
             float getVelocityX() {return vel.x;}
             float getVelocityY() {return vel.y;}

            /*
             * apply a force (such as gravity, friction, or movement) to the entity
             */
            void applyForce(Vec2& force) {vel.x += force.x; vel.y += force.y;}
            void applyForce(float x, float y) {vel.x += x; vel.y += y;}

            /*
             * get the bounding box of the entity.
             */
            SDL_FRect getBoundingBox();

            /*
             * get the width/height of the entity.
             */
            float getWidth();
            float getHeight();

            /*
             * get the self-defined type string of this entity.
             */
             std::string getType() {return type;}
            /*
             * set the type string of this entity.
             */
            void setType(std::string t) {type = t;}

    };
}
