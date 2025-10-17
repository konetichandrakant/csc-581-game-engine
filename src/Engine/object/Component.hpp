#pragma once
#include <string>
#include <typeinfo>
#include <typeindex>

namespace Engine::Obj {
    // Minimal runtime-typed component interface
    struct IComponent {
        virtual ~IComponent() = default;
        virtual std::type_index type() const noexcept = 0;
        virtual const char* name() const noexcept = 0;
    };

    // Helper CRTP to give name()/type() without RTTI string ops sprinkled everywhere
    template <typename T>
    struct Component : IComponent {
        std::type_index type() const noexcept override { return typeid(T); }
        const char* name() const noexcept override { return T::kName; }
    };
}
