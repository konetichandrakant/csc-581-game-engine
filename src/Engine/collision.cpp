#include "collision.h"
#include "core.h"
#include "vec2.h"
#include <SDL3/SDL_rect.h>
#include <cmath>

namespace Engine::Collision {
    bool check(Entity* a, Entity* b) {
        SDL_FRect a_box = a->getBoundingBox();
        SDL_FRect b_box = b->getBoundingBox();
        return SDL_HasRectIntersectionFloat(&a_box, &b_box);
    };

    std::vector<Entity*> all(Entity* e) {
        std::vector<Entity*> out;
        for (auto cmp : entities) {
            if (cmp == e || !cmp->hasCollisions()) continue; // don't check for collisions with yourself, ignore entities with collisions off
            if (check(cmp, e)) out.push_back(cmp);
        }
        return out;
    };

    int _checkEdge_internal(SDL_FRect a_box, SDL_FRect b_box, Vec2 &a_pos, Vec2 &b_pos) {
        SDL_FRect overlap;
        if (!SDL_GetRectIntersectionFloat(&a_box, &b_box, &overlap)) return NO_COLLISION;

        if (overlap.w >= overlap.h) {
            // if our overlap is wider than it is tall, this was very likely a vertical collision.
            // returns 4 (BOTTOM) if a is above b, and 3 (TOP) if a is below b.
            if (a_pos.y < b_pos.y) {
                return BOTTOM;
            } else {
                return TOP;
            }
        } else {
            // otherwise, probably a horizontal collision.
            // returns 2 (RIGHT) if a is to the left of b, and 1 (LEFT) if a is to the right of b.
            if (a_pos.x < b_pos.x) {
                return RIGHT;
            } else {
                return LEFT;
            }
        }
    }

    int checkEdge(Entity* a, Entity* b) {
        /*
         * unfortunately, all of my many hacks were insufficient
         * and we have to actually step 1px at a time through
         * the movement to get accurate collisions. bummer!
         */

        const float STEP_SIZE = 1; //px

        SDL_FRect a_box = a->getBoundingBox();
        SDL_FRect b_box = b->getBoundingBox();

        if (!check(a, b)) return NO_COLLISION;

        Vec2 a_pos = a->getPos();
        Vec2 b_pos = b->getPos();

        Vec2 d_v, step;
        Vec2_sub(a->getVelocity(), b->getVelocity(), d_v);
        Vec2_scale(d_v, timeline->getDelta());
        Vec2_normalize(d_v, step);
        Vec2_scale(step, STEP_SIZE);

        int n = Vec2_length(d_v) / STEP_SIZE;
        int code = 0;

        // starting position of entity A the previous frame
        a_box.x -= d_v.x;
        a_box.y -= d_v.y;

        for (int i = 0; i <= n; i++) {
            code = _checkEdge_internal(a_box, b_box, a_pos, b_pos);
            if (code != NO_COLLISION) return code;

            a_box.x += step.x;
            a_box.y += step.y;
        }

        // it shouldn't be possible to get here
        return NO_COLLISION;
    };
}
