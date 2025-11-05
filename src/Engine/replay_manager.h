#pragma once
#include "event_manager.h"
#include "timeline.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace Engine {
    
    // Recorded event entry - stores event with relative timestamp
    struct RecordedEvent {
        double relativeTimestamp;  // Time relative to recording start
        std::shared_ptr<Event> event;
        
        bool operator<(const RecordedEvent& other) const {
            return relativeTimestamp < other.relativeTimestamp;
        }
    };
    
    class ReplayManager {
    public:
        ReplayManager(EventManager* eventManager, Timeline* timeline);
        
        // Recording control
        void startRecording(const std::string& name);
        void stopRecording();
        bool isRecording() const { return recording_; }
        
        // Playback control
        void startPlayback(const std::string& name);
        void stopPlayback();
        void pausePlayback(bool pause);
        bool isPlaying() const { return playing_; }
        bool isPaused() const { return paused_; }
        double getPlaybackTime() const { return playbackTime_; }
        
        // Seek in playback
        void seek(double time);
        
        // Event capture (called by EventManager)
        void captureEvent(std::shared_ptr<Event> event);
        
        // Update (called each frame) - handles playback timing
        void update();
        
        // Get available recordings
        std::vector<std::string> getRecordingNames() const;
        
        // Clear all recordings
        void clearAllRecordings();
        
    private:
        EventManager* eventManager_;
        Timeline* timeline_;
        
        bool recording_;
        bool playing_;
        bool paused_;
        
        std::string currentRecordingName_;
        double recordingStartTime_;
        std::vector<RecordedEvent> currentRecording_;
        
        std::string currentPlaybackName_;
        std::vector<RecordedEvent> currentPlayback_;
        size_t playbackIndex_;
        double playbackTime_;
        double playbackStartTime_;
        
        std::unordered_map<std::string, std::vector<RecordedEvent>> recordings_;
        
        // Clone an event for storage (simple shallow copy for now)
        std::shared_ptr<Event> cloneEvent(std::shared_ptr<Event> event);
    };
}

