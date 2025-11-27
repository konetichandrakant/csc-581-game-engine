
#pragma once
#include "NetworkSceneManager.hpp"
#include <unordered_map>
#include <queue>
#include <chrono>

namespace Engine::Obj {

struct InputDelta {
    uint64_t timestamp;
    uint32_t playerId;
    uint8_t inputFlags;
    uint8_t sequence;
};

class InputDeltaNetwork {
public:
    InputDeltaNetwork(Registry& registry);
    

    void sendInputDelta(uint32_t playerId, bool left, bool right, bool jump);
    

    void processInputDeltas();
    

    void reconstructPlayerState(uint32_t playerId);
    

    size_t getTotalBytesSent() const { return totalBytesSent_; }
    size_t getTotalMessagesSent() const { return totalMessagesSent_; }
    double getAverageLatency() const { return avgLatencyMs_; }
    
private:
    Registry& registry_;
    

    std::unordered_map<uint32_t, uint8_t> lastInputState_;
    std::unordered_map<uint32_t, uint8_t> currentInputState_;
    std::unordered_map<uint32_t, uint8_t> inputSequence_;
    

    std::queue<InputDelta> outgoingDeltas_;
    std::queue<InputDelta> incomingDeltas_;
    

    size_t totalBytesSent_{0};
    size_t totalMessagesSent_{0};
    double avgLatencyMs_{0.0};
    

    struct PlayerState {
        float x, y, vx, vy;
        uint64_t lastUpdate;
    };
    std::unordered_map<uint32_t, PlayerState> reconstructedStates_;
    

    uint8_t packInputFlags(bool left, bool right, bool jump);
    void unpackInputFlags(uint8_t flags, bool& left, bool& right, bool& jump);
    void applyInputToState(uint32_t playerId, uint8_t inputFlags, double dt);
};

}