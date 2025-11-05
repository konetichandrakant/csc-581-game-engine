#pragma once
#include <string>
#include <typeinfo>
#include <typeindex>

namespace Engine::Obj {

    struct IComponent {
        virtual ~IComponent() = default;
        virtual std::type_index type() const noexcept = 0;
        virtual const char* name() const noexcept = 0;
    };


    template <typename T>
    struct Component : IComponent {
        std::type_index type() const noexcept override { return typeid(T); }
        const char* name() const noexcept override { return T::kName; }
    };
}
