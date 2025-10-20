// src/Engine/object/components/NetworkPlayer.hpp
#pragma once
#include "../Component.hpp"
#include <chrono>

namespace Engine::Obj {
    struct NetworkPlayer final : Component<NetworkPlayer> {
        static constexpr const char* kName = "NetworkPlayer";
        
        int playerId{0};
        bool isLocal{false};
        bool isConnected{true};
        
        // Position and velocity
        float x{0.f}, y{0.f}, vx{0.f}, vy{0.f};
        uint8_t facing{0}, anim{0};
        uint64_t lastTick{0};
        
        // Timing for disconnect detection
        std::chrono::steady_clock::time_point lastUpdateTime;
        static constexpr auto DISCONNECT_TIMEOUT = std::chrono::milliseconds(5000);
        
        // Input state for local players
        bool leftPressed{false}, rightPressed{false}, jumpPressed{false};
        
        // Network interpolation
        float targetX{0.f}, targetY{0.f};
        float prevX{0.f}, prevY{0.f};
        
        bool isDisconnected() const {
            if (isLocal) return false;
            auto now = std::chrono::steady_clock::now();
            return (now - lastUpdateTime) > DISCONNECT_TIMEOUT;
        }
        
        void updateNetworkState(float newX, float newY, float newVx, float newVy, 
                               uint8_t newFacing, uint8_t newAnim, uint64_t tick) {
            if (!isLocal) {
                prevX = x;
                prevY = y;
                targetX = newX;
                targetY = newY;
                vx = newVx;
                vy = newVy;
                facing = newFacing;
                anim = newAnim;
                lastTick = tick;
                lastUpdateTime = std::chrono::steady_clock::now();
                isConnected = true;
            }
        }
    };
}