#pragma once
#include <cstddef>
#include <memory>
#include <unordered_map>
#include "Types.hpp"
#include "GameObject.hpp"
#include "Engine/memory/MemoryManager.hpp"

namespace Engine::Obj {

    class Registry {
    public:
        using GameObjectPtr = std::unique_ptr<GameObject, Memory::PoolDeleter<GameObject>>;

        GameObject& create() {
            ObjectId next = ++lastId_;
            GameObject* raw = Memory::MemoryManager::instance().create<GameObject>(next);
            objects_.emplace(next, GameObjectPtr(raw));
            return *raw;
        }

        bool destroy(ObjectId id) {
            return objects_.erase(id) > 0;
        }

        GameObject* get(ObjectId id) {
            auto it = objects_.find(id);
            return (it == objects_.end()) ? nullptr : it->second.get();
        }

        const GameObject* get(ObjectId id) const {
            auto it = objects_.find(id);
            return (it == objects_.end()) ? nullptr : it->second.get();
        }

        template <typename Fn>
        void forEach(Fn&& fn) {
            for (auto& kv : objects_) fn(*kv.second);
        }

        void reserveGameObjects(std::size_t count) {
            Memory::MemoryManager::instance().configurePool<GameObject>(count);
        }

        Memory::PoolStats gameObjectPoolStats() const {
            return Memory::MemoryManager::instance().stats<GameObject>();
        }

        std::size_t size() const noexcept { return objects_.size(); }

    private:
        ObjectId lastId_{0};
        std::unordered_map<ObjectId, GameObjectPtr> objects_;
    };

}
