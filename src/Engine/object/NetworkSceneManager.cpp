
#include "NetworkSceneManager.hpp"
#include <algorithm>
#include <vector>

namespace Engine::Obj {
    
    ObjectId NetworkSceneManager::createLocalPlayer(int playerId, float x, float y, const std::string& spritePath) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& player = registry_.create();
        auto playerId_obj = player.id();
        

        auto& transform = player.add<Transform>();
        transform.x = x;
        transform.y = y;
        
        auto& sprite = player.add<Sprite>();
        sprite.textureKey = spritePath;
        sprite.visible = true;
        
        auto& physics = player.add<PhysicsBody2D>();
        physics.isKinematic = false;
        
        auto& network = player.add<NetworkPlayer>();
        network.playerId = playerId;
        network.isLocal = true;
        network.isConnected = true;
        network.x = x;
        network.y = y;
        network.lastUpdateTime = std::chrono::steady_clock::now();
        
        networkIdToObjectId_[playerId] = playerId_obj;
        objectIdToNetworkId_[playerId_obj] = playerId;
        
        return playerId_obj;
    }
    
    ObjectId NetworkSceneManager::createOrUpdateRemotePlayer(int playerId, float x, float y, 
                                                           float vx, float vy, uint8_t facing, 
                                                           uint8_t anim, uint64_t tick) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = networkIdToObjectId_.find(playerId);
        if (it != networkIdToObjectId_.end()) {

            auto* player = registry_.get(it->second);
            if (player) {
                auto* network = player->get<NetworkPlayer>();
                if (network) {
                    network->updateNetworkState(x, y, vx, vy, facing, anim, tick);
                }
            }
            return it->second;
        } else {

            auto& player = registry_.create();
            auto playerId_obj = player.id();
            

            auto& transform = player.add<Transform>();
            transform.x = x;
            transform.y = y;
            
            auto& sprite = player.add<Sprite>();
            sprite.textureKey = "media/hurst.png";
            sprite.visible = true;
            
            auto& physics = player.add<PhysicsBody2D>();
            physics.isKinematic = true;
            
            auto& network = player.add<NetworkPlayer>();
            network.playerId = playerId;
            network.isLocal = false;
            network.isConnected = true;
            network.updateNetworkState(x, y, vx, vy, facing, anim, tick);
            
            networkIdToObjectId_[playerId] = playerId_obj;
            objectIdToNetworkId_[playerId_obj] = playerId;
            
            return playerId_obj;
        }
    }
    
    void NetworkSceneManager::updateLocalPlayerInput(ObjectId playerId, bool left, bool right, bool jump) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto* player = registry_.get(playerId);
        if (player) {
            auto* network = player->get<NetworkPlayer>();
            if (network && network->isLocal) {
                network->leftPressed = left;
                network->rightPressed = right;
                network->jumpPressed = jump;
            }
        }
    }
    
    std::vector<ObjectId> NetworkSceneManager::getAllNetworkPlayers() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<ObjectId> players;
        players.reserve(networkIdToObjectId_.size());
        
        for (const auto& [networkId, objectId] : networkIdToObjectId_) {
            players.push_back(objectId);
        }
        
        return players;
    }
    
    void NetworkSceneManager::cleanupDisconnectedPlayers() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<ObjectId> toRemove;
        
        for (const auto& [networkId, objectId] : networkIdToObjectId_) {
            auto* player = registry_.get(objectId);
            if (player) {
                auto* network = player->get<NetworkPlayer>();
                if (network && network->isDisconnected()) {
                    toRemove.push_back(objectId);
                }
            }
        }
        
        for (ObjectId id : toRemove) {
            removePlayer(id);
        }
    }
    
    ObjectId NetworkSceneManager::getPlayerByNetworkId(int networkId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = networkIdToObjectId_.find(networkId);
        return (it != networkIdToObjectId_.end()) ? it->second : kInvalidId;
    }
    
    void NetworkSceneManager::removePlayer(ObjectId playerId) {
        auto it = objectIdToNetworkId_.find(playerId);
        if (it != objectIdToNetworkId_.end()) {
            int networkId = it->second;
            networkIdToObjectId_.erase(networkId);
            objectIdToNetworkId_.erase(playerId);
        }
        
        registry_.destroy(playerId);
    }
}