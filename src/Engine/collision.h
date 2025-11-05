#pragma once

#include "entity.h"
#include <vector>

namespace Engine::Collision {

 
    bool check(Entity* a, Entity* b);

    
  
    std::vector<Entity*> all(Entity* e);


 
    const int NO_COLLISION = 0;
    const int LEFT = 1;
    const int RIGHT = 2;
    const int TOP = 3;
    const int BOTTOM = 4;


    int checkEdge(Entity* a, Entity* b);
}
