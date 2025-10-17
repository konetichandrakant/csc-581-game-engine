#pragma once
#include "../Component.hpp"

namespace Engine::Obj {
    struct PhysicsBody2D final : Component<PhysicsBody2D> {
        static constexpr const char* kName = "PhysicsBody2D";
        float vx{0.f}, vy{0.f};
        float ax{0.f}, ay{0.f};
        float mass{1.f};
        bool  isKinematic{false}; // if true, ignore forces/integration
    };
}
