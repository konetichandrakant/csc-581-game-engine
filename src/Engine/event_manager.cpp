#include "event_manager.h"
#include "events.h"
#include "replay_manager.h"
#include <algorithm>

namespace Engine {

    EventManager::EventManager(Timeline* timeline) 
        : timeline(timeline), replayManager_(nullptr), nextHandlerId(1) {
    }

    void EventManager::setReplayManager(ReplayManager* replayManager) {
        replayManager_ = replayManager;
    }

    HandlerId EventManager::registerHandler(const std::string& eventType, EventHandler handler) {
        HandlerId id = nextHandlerId++;
        handlers[id] = {handler, eventType};
        typeToHandlers[eventType].push_back(id);
        return id;
    }

    void EventManager::unregisterHandler(HandlerId id) {
        auto it = handlers.find(id);
        if (it != handlers.end()) {
            std::string eventType = it->second.eventType;
            handlers.erase(it);
            
            // Remove from type map
            auto& vec = typeToHandlers[eventType];
            vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
        }
    }

    void EventManager::raise(std::shared_ptr<Event> event) {
        if (!event) return;
        
        // Set timestamp using timeline
        event->timestamp = timeline->now();
        
        // Capture for replay if recording
        if (replayManager_) {
            replayManager_->captureEvent(event);
        }
        
        // Dispatch immediately
        dispatch(event->getType(), event);
    }

    void EventManager::queue(std::shared_ptr<Event> event) {
        if (!event) return;
        
        // Set timestamp using timeline
        event->timestamp = timeline->now();
        
        // Add to priority queue
        eventQueue.push({event, event->timestamp});
    }

    void EventManager::process() {
        double currentTime = timeline->now();
        
        // Process all events whose timestamp has arrived
        while (!eventQueue.empty() && eventQueue.top().timestamp <= currentTime) {
            auto queuedEvent = eventQueue.top();
            eventQueue.pop();
            
            dispatch(queuedEvent.event->getType(), queuedEvent.event);
        }
    }

    void EventManager::dispatch(const std::string& eventType, std::shared_ptr<Event> event) {
        auto it = typeToHandlers.find(eventType);
        if (it != typeToHandlers.end()) {
            // Call all registered handlers for this event type
            for (HandlerId id : it->second) {
                auto handlerIt = handlers.find(id);
                if (handlerIt != handlers.end()) {
                    handlerIt->second.handler(event);
                }
            }
        }
    }

    void EventManager::clear() {
        handlers.clear();
        typeToHandlers.clear();
        
        // Clear queue
        while (!eventQueue.empty()) {
            eventQueue.pop();
        }
    }

}

