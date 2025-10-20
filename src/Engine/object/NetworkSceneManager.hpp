// src/Engine/object/NetworkSceneManager.hpp
#pragma once
#include "Registry.hpp"
#include "components/NetworkPlayer.hpp"
#include "components/Transform.hpp"
#include "components/Sprite.hpp"
#include "components/PhysicsBody2D.hpp"
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace Engine::Obj {
    class NetworkSceneManager {
    public:
        NetworkSceneManager(Registry& registry) : registry_(registry) {}
        
        // Create local player
        ObjectId createLocalPlayer(int playerId, float x, float y, const std::string& spritePath);
        
        // Create/update remote player
        ObjectId createOrUpdateRemotePlayer(int playerId, float x, float y, float vx, float vy,
                                          uint8_t facing, uint8_t anim, uint64_t tick);
        
        // Update local player input
        void updateLocalPlayerInput(ObjectId playerId, bool left, bool right, bool jump);
        
        // Get all network players
        std::vector<ObjectId> getAllNetworkPlayers() const;
        
        // Clean up disconnected players
        void cleanupDisconnectedPlayers();
        
        // Get player by network ID
        ObjectId getPlayerByNetworkId(int networkId) const;
        
        // Remove player
        void removePlayer(ObjectId playerId);
        
    private:
        Registry& registry_;
        mutable std::mutex mutex_;
        std::unordered_map<int, ObjectId> networkIdToObjectId_;
        std::unordered_map<ObjectId, int> objectIdToNetworkId_;
    };
}