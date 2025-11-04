#include "replay_manager.h"
#include "events.h"
#include "replay_events.h"
#include <algorithm>
#include <iostream>

namespace Engine {

    ReplayManager::ReplayManager(EventManager* eventManager, Timeline* timeline)
        : eventManager_(eventManager), timeline_(timeline),
          recording_(false), playing_(false), paused_(false),
          playbackIndex_(0), playbackTime_(0.0), playbackStartTime_(0.0) {
    }

    void ReplayManager::startRecording(const std::string& name) {
        if (recording_) {
            std::cerr << "[Replay] Already recording. Stop first." << std::endl;
            return;
        }
        
        if (playing_) {
            std::cerr << "[Replay] Cannot record while playing." << std::endl;
            return;
        }
        
        recording_ = true;
        currentRecordingName_ = name;
        recordingStartTime_ = timeline_->now();
        currentRecording_.clear();
        
        std::cout << "[Replay] Started recording: " << name << std::endl;
    }

    void ReplayManager::stopRecording() {
        if (!recording_) {
            std::cerr << "[Replay] Not recording." << std::endl;
            return;
        }
        
        recording_ = false;
        
        // Store the recording
        if (!currentRecording_.empty()) {
            recordings_[currentRecordingName_] = currentRecording_;
            std::cout << "[Replay] Stopped recording: " << currentRecordingName_ 
                      << " (" << currentRecording_.size() << " events)" << std::endl;
        }
        
        currentRecording_.clear();
        currentRecordingName_ = "";
    }

    void ReplayManager::startPlayback(const std::string& name) {
        if (recording_) {
            std::cerr << "[Replay] Cannot play while recording." << std::endl;
            return;
        }
        
        if (playing_) {
            stopPlayback();
        }
        
        auto it = recordings_.find(name);
        if (it == recordings_.end()) {
            std::cerr << "[Replay] Recording not found: " << name << std::endl;
            return;
        }
        
        playing_ = true;
        paused_ = false;
        currentPlaybackName_ = name;
        currentPlayback_ = it->second;
        playbackIndex_ = 0;
        playbackStartTime_ = timeline_->now();
        playbackTime_ = 0.0;
        
        std::cout << "[Replay] Started playback: " << name 
                  << " (" << currentPlayback_.size() << " events)" << std::endl;
    }

    void ReplayManager::stopPlayback() {
        if (!playing_) return;
        
        playing_ = false;
        paused_ = false;
        playbackIndex_ = 0;
        playbackTime_ = 0.0;
        currentPlayback_.clear();
        currentPlaybackName_ = "";
        
        std::cout << "[Replay] Stopped playback" << std::endl;
    }

    void ReplayManager::pausePlayback(bool pause) {
        if (!playing_) return;
        
        paused_ = pause;
        
        if (pause) {
            // Store the current playback time
            playbackTime_ = timeline_->now() - playbackStartTime_;
            std::cout << "[Replay] Paused playback at time: " << playbackTime_ << std::endl;
        } else {
            // Resume from stored time
            playbackStartTime_ = timeline_->now() - playbackTime_;
            std::cout << "[Replay] Resumed playback from time: " << playbackTime_ << std::endl;
        }
    }

    void ReplayManager::seek(double time) {
        if (!playing_) return;
        
        playbackTime_ = time;
        playbackStartTime_ = timeline_->now() - playbackTime_;
        
        // Find the index for the new time
        playbackIndex_ = 0;
        for (size_t i = 0; i < currentPlayback_.size(); ++i) {
            if (currentPlayback_[i].relativeTimestamp <= time) {
                playbackIndex_ = i + 1;
            } else {
                break;
            }
        }
        
        std::cout << "[Replay] Seeked to time: " << time << std::endl;
    }

    void ReplayManager::captureEvent(std::shared_ptr<Event> event) {
        if (!recording_ || !event) return;
        
        // Skip replay control events to avoid infinite loops
        std::string type = event->getType();
        if (type == "start_recording" || type == "stop_recording" ||
            type == "start_playback" || type == "stop_playback" ||
            type == "pause_playback" || type == "seek_playback") {
            return;
        }
        
        double relativeTime = timeline_->now() - recordingStartTime_;
        
        RecordedEvent recorded;
        recorded.relativeTimestamp = relativeTime;
        recorded.event = event;  // Store shared pointer directly
        
        currentRecording_.push_back(recorded);
    }

    void ReplayManager::update() {
        if (!playing_ || paused_) return;
        
        double currentPlaybackTime = timeline_->now() - playbackStartTime_;
        playbackTime_ = currentPlaybackTime;
        
        // Play events whose time has arrived
        while (playbackIndex_ < currentPlayback_.size()) {
            const RecordedEvent& recorded = currentPlayback_[playbackIndex_];
            
            if (recorded.relativeTimestamp <= currentPlaybackTime) {
                // Replay this event
                if (eventManager_ && recorded.event) {
                    // Reconstruct event with current timeline time
                    recorded.event->timestamp = timeline_->now();
                    std::cout << "[Replay] Playing event: " << recorded.event->getType() 
                              << " at time " << recorded.relativeTimestamp << " (playback time: " 
                              << currentPlaybackTime << ")" << std::endl;
                    eventManager_->raise(recorded.event);
                }
                
                playbackIndex_++;
            } else {
                // Not time for next event yet
                break;
            }
        }
        
        // Check if playback is complete
        if (playbackIndex_ >= currentPlayback_.size()) {
            std::cout << "[Replay] Playback complete" << std::endl;
            stopPlayback();
        }
    }

    std::vector<std::string> ReplayManager::getRecordingNames() const {
        std::vector<std::string> names;
        for (const auto& pair : recordings_) {
            names.push_back(pair.first);
        }
        return names;
    }

    void ReplayManager::clearAllRecordings() {
        recordings_.clear();
        currentRecording_.clear();
        std::cout << "[Replay] Cleared all recordings" << std::endl;
    }

    std::shared_ptr<Event> ReplayManager::cloneEvent(std::shared_ptr<Event> event) {
        // Simple implementation - just return the shared pointer
        // For a real system, you'd want deep cloning
        return event;
    }
}

