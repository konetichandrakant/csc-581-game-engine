#include "physics.h"
#include <cstdlib>
#include <cmath>

namespace Engine {

    void Physics::apply(Entity * e, float dt) {
        // apply force of gravity
        if (e->hasGravity()) {
            e->applyForce(0, gravity * dt);
        }

        // apply force of friction
        Vec2 fric = e->getFriction();
        Vec2 vel = e->getVelocity();


        // compute the components of the resulting friction force.
        // friction opposes the direction of motion, and should
        // be clamped so that the direction of motion never reverses.
        float f_x = clamps(vel.x, fric.x * dt);
        float f_y = clamps(vel.y, fric.y * dt);

        e->applyForce(-f_x, -f_y);

        vel = e->getVelocity();
        Vec2 maxVel = e->getMaxSpeed();

        // ensure velocity doesn't exceed the maximum speed in either direction
        float v_x = vel.x;
        float v_y = vel.y;
        if (maxVel.x > 0) v_x = clamps(vel.x, maxVel.x);
        if (maxVel.y > 0) v_y = clamps(vel.y, maxVel.y);
        e->setVelocity(v_x, v_y);

        // do the actual kinematics part (move by velocity)
        e->translate(v_x * dt, v_y * dt);
    }
}
