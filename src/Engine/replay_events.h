#pragma once
#include "event.h"
#include <string>

namespace Engine {
    // Start Recording Event
    class StartRecordingEvent : public Event {
    public:
        std::string recordingName;
        
        StartRecordingEvent(const std::string& name = "default") 
            : recordingName(name) {}
        
        std::string getType() const override { return "start_recording"; }
    };

    // Stop Recording Event
    class StopRecordingEvent : public Event {
    public:
        std::string recordingName;
        
        StopRecordingEvent(const std::string& name = "default") 
            : recordingName(name) {}
        
        std::string getType() const override { return "stop_recording"; }
    };

    // Start Playback Event
    class StartPlaybackEvent : public Event {
    public:
        std::string recordingName;
        
        StartPlaybackEvent(const std::string& name = "default") 
            : recordingName(name) {}
        
        std::string getType() const override { return "start_playback"; }
    };

    // Stop Playback Event
    class StopPlaybackEvent : public Event {
    public:
        StopPlaybackEvent() {}
        
        std::string getType() const override { return "stop_playback"; }
    };

    // Pause Playback Event
    class PausePlaybackEvent : public Event {
    public:
        bool pause;
        
        PausePlaybackEvent(bool p) : pause(p) {}
        
        std::string getType() const override { return "pause_playback"; }
    };

    // Seek Playback Event
    class SeekPlaybackEvent : public Event {
    public:
        double seekTime;
        
        SeekPlaybackEvent(double time) : seekTime(time) {}
        
        std::string getType() const override { return "seek_playback"; }
    };
}

