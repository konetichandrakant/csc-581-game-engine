#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <typeindex>
#include "Types.hpp"
#include "Component.hpp"

namespace Engine::Obj {

    using Property = std::variant<bool,int,float,double,std::string>;
    using PropertyMap = std::unordered_map<std::string, Property>;

    class GameObject {
    public:
        explicit GameObject(ObjectId id) : id_(id) {}

        ObjectId id() const noexcept { return id_; }


        template <typename T, typename... Args>
        T& add(Args&&... args) {
            static_assert(std::is_base_of_v<IComponent,T>, "T must be a component");
            auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
            auto key = std::type_index(typeid(T));
            auto* raw = ptr.get();
            components_[key] = std::move(ptr);
            return *raw;
        }

        template <typename T>
        [[nodiscard]] bool has() const {
            auto key = std::type_index(typeid(T));
            return components_.find(key) != components_.end();
        }

        template <typename T>
        T* get() {
            auto key = std::type_index(typeid(T));
            auto it = components_.find(key);
            if (it == components_.end()) return nullptr;
            return static_cast<T*>(it->second.get());
        }

        template <typename T>
        const T* get() const {
            auto key = std::type_index(typeid(T));
            auto it = components_.find(key);
            if (it == components_.end()) return nullptr;
            return static_cast<const T*>(it->second.get());
        }


        void setProperty(const std::string& key, Property value) {
            props_[key] = std::move(value);
        }

        const Property* getProperty(const std::string& key) const {
            auto it = props_.find(key);
            return (it == props_.end()) ? nullptr : &it->second;
        }

        bool hasProperty(const std::string& key) const {
            return props_.find(key) != props_.end();
        }

    private:
        ObjectId id_{kInvalidId};
        std::unordered_map<std::type_index, std::unique_ptr<IComponent>> components_;
        PropertyMap props_;
    };
}
