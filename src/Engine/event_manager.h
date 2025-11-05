#pragma once

#include "event.h"
#include "timeline.h"
#include <functional>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>

namespace Engine {

    // Forward declaration
    class ReplayManager;

    // Handler function type
    using EventHandler = std::function<void(std::shared_ptr<Event>)>;
    using HandlerId = size_t;

    class EventManager {
    public:
        EventManager(Timeline* timeline);
        
        // Register a handler for a specific event type
        HandlerId registerHandler(const std::string& eventType, EventHandler handler);
        
        // Unregister a handler using its ID
        void unregisterHandler(HandlerId id);
        
        // Raise an event (immediate dispatch)
        void raise(std::shared_ptr<Event> event);
        
        // Queue an event for later processing (uses timeline timestamp)
        void queue(std::shared_ptr<Event> event);
        
        // Process all queued events
        void process();
        
        // Clear all handlers
        void clear();
        
        // Set ReplayManager for event capture
        void setReplayManager(ReplayManager* replayManager);
        
    private:
        struct QueuedEvent {
            std::shared_ptr<Event> event;
            double timestamp;
            
            bool operator>(const QueuedEvent& other) const {
                return timestamp > other.timestamp; // Min heap
            }
        };
        
        struct HandlerEntry {
            EventHandler handler;
            std::string eventType;
        };
        
        Timeline* timeline;
        ReplayManager* replayManager_;
        std::unordered_map<std::string, std::vector<HandlerId>> typeToHandlers;
        std::unordered_map<HandlerId, HandlerEntry> handlers;
        std::priority_queue<QueuedEvent, std::vector<QueuedEvent>, std::greater<QueuedEvent>> eventQueue;
        HandlerId nextHandlerId;
        
        void dispatch(const std::string& eventType, std::shared_ptr<Event> event);
    };

}

