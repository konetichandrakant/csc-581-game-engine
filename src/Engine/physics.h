#pragma once
#include "entity.h"
#include "vec2.h"
#include <cmath>
#include <algorithm>

namespace Engine {
    class Physics {
        private:
            /*
             * strength of gravity in pixels/sec^2.
             */
            static inline float gravity = 2000;

            /*
             * utility function for clamping a signed value while preserving the sign
             * (useful for friction and max speed calculations.)
             */
            static float clamps(float value, float max) {
                return std::copysignf(std::min(std::abs(value), max), value);
            }

        public:

            /*
             * apply physics to an entity given the time step. called automatically by Engine::main().
             */
            static void apply(Entity* e, float dt);

            /*
             * set the global strength of gravity.
             */
            static void setGravity(float g) {gravity = g;}

            /*
             * get the current strength of gravity
             */
            static float getGravity() {return gravity;}
    };
}
