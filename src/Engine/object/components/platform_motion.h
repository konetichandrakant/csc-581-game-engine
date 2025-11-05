#pragma once
#include <cmath>
#include <cstdint>

namespace Engine::Obj {

enum class MotionType : uint8_t { Horizontal=0, Vertical=1 };

struct PlatformMotion {
    MotionType type{MotionType::Horizontal};


    float minX{0}, maxX{0};
    float minY{0}, maxY{0};


    float vx{0}, vy{0};


    template<typename TransformT>
    void step(TransformT& tr, float dt) {
        switch (type) {
            case MotionType::Horizontal:
                tr.pos.x += vx * dt;
                if (tr.pos.x < minX) { tr.pos.x = minX; vx = std::fabs(vx); }
                if (tr.pos.x > maxX) { tr.pos.x = maxX; vx = -std::fabs(vx); }
                break;
            case MotionType::Vertical:
                tr.pos.y += vy * dt;
                if (tr.pos.y < minY) { tr.pos.y = minY; vy = std::fabs(vy); }
                if (tr.pos.y > maxY) { tr.pos.y = maxY; vy = -std::fabs(vy); }
                break;
        }
    }
};

}
