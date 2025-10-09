#pragma once

#include "entity.h"
#include <vector>

namespace Engine::Collision {

    /*
     * check if the bounding box of entities a and b are overlapping.
     */
    bool check(Entity* a, Entity* b);

    /*
     * check if entity e is colliding with any entities from the global list.
     *
     * returns a vector of all entities that are overlapping.
     */
    std::vector<Entity*> all(Entity* e);


    /*
     * codes returned by checkEdge.
     */
    const int NO_COLLISION = 0;
    const int LEFT = 1;
    const int RIGHT = 2;
    const int TOP = 3;
    const int BOTTOM = 4;

    /*
     * returns an int code indicating which edge of entity a is intersecting entity b.
     *
     * NOTE: this function is more costly than check(), prefer that function
     * if it works for your use case!
     */
    int checkEdge(Entity* a, Entity* b);
}
