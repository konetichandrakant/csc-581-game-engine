#pragma once
#include "../Component.hpp"

namespace Engine::Obj {
    struct Transform final : Component<Transform> {
        static constexpr const char* kName = "Transform";
        float x{0.f}, y{0.f};
        float rotationDeg{0.f};
        float sx{1.f}, sy{1.f};
    };
}
