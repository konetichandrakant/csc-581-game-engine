#include "physics.h"
#include <cstdlib>
#include <cmath>

namespace Engine {

    void Physics::apply(Entity * e, float dt) {

        if (e->hasGravity()) {
            e->applyForce(0, gravity * dt);
        }


        Vec2 fric = e->getFriction();
        Vec2 vel = e->getVelocity();





        float f_x = clamps(vel.x, fric.x * dt);
        float f_y = clamps(vel.y, fric.y * dt);

        e->applyForce(-f_x, -f_y);

        vel = e->getVelocity();
        Vec2 maxVel = e->getMaxSpeed();


        float v_x = vel.x;
        float v_y = vel.y;
        if (maxVel.x > 0) v_x = clamps(vel.x, maxVel.x);
        if (maxVel.y > 0) v_y = clamps(vel.y, maxVel.y);
        e->setVelocity(v_x, v_y);


        e->translate(v_x * dt, v_y * dt);
    }
}
