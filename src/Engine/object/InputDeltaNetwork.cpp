
#include "InputDeltaNetwork.hpp"
#include <algorithm>
#include <cmath>

namespace Engine::Obj {

InputDeltaNetwork::InputDeltaNetwork(Registry& registry) : registry_(registry) {}

void InputDeltaNetwork::sendInputDelta(uint32_t playerId, bool left, bool right, bool jump) {
    uint8_t currentFlags = packInputFlags(left, right, jump);
    uint8_t lastFlags = lastInputState_[playerId];
    

    if (currentFlags != lastFlags) {
        InputDelta delta;
        delta.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        delta.playerId = playerId;
        delta.inputFlags = currentFlags;
        delta.sequence = ++inputSequence_[playerId];
        
        outgoingDeltas_.push(delta);
        lastInputState_[playerId] = currentFlags;
        

        totalBytesSent_ += sizeof(InputDelta);
        totalMessagesSent_++;
    }
}

void InputDeltaNetwork::processInputDeltas() {
    while (!incomingDeltas_.empty()) {
        InputDelta delta = incomingDeltas_.front();
        incomingDeltas_.pop();
        

        bool left, right, jump;
        unpackInputFlags(delta.inputFlags, left, right, jump);
        

        double dt = 1.0 / 120.0;
        applyInputToState(delta.playerId, delta.inputFlags, dt);
        

        auto now = std::chrono::steady_clock::now();
        auto sentTime = std::chrono::steady_clock::time_point(
            std::chrono::milliseconds(delta.timestamp));
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(now - sentTime);
        avgLatencyMs_ = (avgLatencyMs_ + latency.count() / 1000.0) / 2.0;
    }
}

void InputDeltaNetwork::reconstructPlayerState(uint32_t playerId) {
    auto* player = registry_.get(playerId);
    if (!player) return;
    
    auto* transform = player->get<Transform>();
    auto* physics = player->get<PhysicsBody2D>();
    auto* network = player->get<NetworkPlayer>();
    
    if (!transform || !physics || !network) return;
    

    uint8_t inputFlags = currentInputState_[playerId];
    bool left, right, jump;
    unpackInputFlags(inputFlags, left, right, jump);
    

    const float ACCEL_X = 1800.0f;
    const float FRICTION_X = 1400.0f;
    const float GRAVITY_Y = 2800.0f;
    const float MAX_VX = 650.0f;
    const float MAX_VY = 2000.0f;
    const float JUMP_VEL = 1550.0f;
    
    double dt = 1.0 / 120.0;
    

    if (left) {
        physics->vx -= ACCEL_X * dt;
    } else if (right) {
        physics->vx += ACCEL_X * dt;
    } else {
        float friction = FRICTION_X * dt;
        if (physics->vx > 0.0f) {
            physics->vx = std::max(0.0f, physics->vx - friction);
        } else if (physics->vx < 0.0f) {
            physics->vx = std::min(0.0f, physics->vx + friction);
        }
    }
    

    physics->vy += GRAVITY_Y * dt;
    

    physics->vx = std::clamp(physics->vx, -MAX_VX, MAX_VX);
    physics->vy = std::clamp(physics->vy, -MAX_VY, MAX_VY);
    

    transform->x += physics->vx * dt;
    transform->y += physics->vy * dt;
    

    network->x = transform->x;
    network->y = transform->y;
    network->vx = physics->vx;
    network->vy = physics->vy;
}

uint8_t InputDeltaNetwork::packInputFlags(bool left, bool right, bool jump) {
    uint8_t flags = 0;
    if (left) flags |= 0x01;
    if (right) flags |= 0x02;
    if (jump) flags |= 0x04;
    return flags;
}

void InputDeltaNetwork::unpackInputFlags(uint8_t flags, bool& left, bool& right, bool& jump) {
    left = (flags & 0x01) != 0;
    right = (flags & 0x02) != 0;
    jump = (flags & 0x04) != 0;
}

void InputDeltaNetwork::applyInputToState(uint32_t playerId, uint8_t inputFlags, double dt) {
    currentInputState_[playerId] = inputFlags;
    reconstructPlayerState(playerId);
}

}