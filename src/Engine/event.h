#pragma once

#include "timeline.h"
#include <string>
#include <memory>
#include <functional>

namespace Engine {

    // Base event class - all events inherit from this
    class Event {
    public:
        virtual ~Event() = default;
        
        // Get the event type as a string identifier
        virtual std::string getType() const = 0;
        
        // Priority timestamp (smaller = higher priority)
        double timestamp;
        
        // Optional event ID for tracking
        size_t eventId;
        
    protected:
        Event() : timestamp(0.0), eventId(0) {}
    };

    // Forward declarations for specific event types
    class CollisionEvent;
    class DeathEvent;
    class SpawnEvent;
    class InputEvent;
}

