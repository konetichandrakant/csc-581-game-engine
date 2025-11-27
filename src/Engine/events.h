#pragma once

#include "event.h"
#include "entity.h"
#include <string>
#include <utility>

namespace Engine {

    // Collision Event
    class CollisionEvent : public Event {
    public:
        Entity* entity1;
        Entity* entity2;
        
        CollisionEvent(Entity* e1, Entity* e2) 
            : entity1(e1), entity2(e2) {}
        
        std::string getType() const override { return "collision"; }
    };

    // Death Event
    class DeathEvent : public Event {
    public:
        Entity* entity;
        std::string cause;
        
        DeathEvent(Entity* e, const std::string& c = "unknown") 
            : entity(e), cause(c) {}
        
        std::string getType() const override { return "death"; }
    };

    // Spawn Event
    class SpawnEvent : public Event {
    public:
        Entity* entity;
        float x, y;
        
        SpawnEvent(Entity* e, float xPos, float yPos) 
            : entity(e), x(xPos), y(yPos) {}
        
        std::string getType() const override { return "spawn"; }
    };

    // Input Event
    class InputEvent : public Event {
    public:
        std::string action;
        bool pressed;
        float duration;
        
        InputEvent(const std::string& act, bool press, float dur = 0.0f) 
            : action(act), pressed(press), duration(dur) {}
        
        std::string getType() const override { return "input"; }
    };

    class InputChordEvent : public Event {
    public:
        std::string chord;
        float held;

        InputChordEvent(std::string chordName, float heldTime = 0.0f)
            : chord(std::move(chordName)), held(heldTime) {}

        std::string getType() const override { return "input_chord"; }
    };
}
