#pragma once
#include <unordered_map>
#include <memory>
#include "Types.hpp"
#include "GameObject.hpp"

namespace Engine::Obj {

    class Registry {
    public:
        GameObject& create() {
            ObjectId next = ++lastId_;
            auto obj = std::make_unique<GameObject>(next);
            auto* raw = obj.get();
            objects_.emplace(next, std::move(obj));
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

        std::size_t size() const noexcept { return objects_.size(); }

    private:
        ObjectId lastId_{0};
        std::unordered_map<ObjectId, std::unique_ptr<GameObject>> objects_;
    };

}
