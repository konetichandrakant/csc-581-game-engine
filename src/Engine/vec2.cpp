#include "vec2.h"
#include <cmath>
#include <cstdio>

namespace Engine {
    void Vec2_add(Vec2 &a, Vec2 &b, Vec2 &out) {
        out.x = a.x + b.x;
        out.y = a.y + b.y;
    };

    void Vec2_sub(Vec2 &a, Vec2 &b, Vec2 &out) {
        out.x = a.x - b.x;
        out.y = a.y - b.y;
    }

    float Vec2_length(Vec2 &vec) {
        return std::sqrt(vec.x * vec.x + vec.y * vec.y);
    }

    void Vec2_scale(Vec2 &vec, float scale, Vec2 &out) {
        out.x = scale * vec.x;
        out.y = scale * vec.y;
    }

    void Vec2_scale(Vec2 &vec, float scale) {
        Vec2_scale(vec, scale, vec);
    }

    void Vec2_normalize(Vec2 &vec, Vec2 &out) {
        Vec2_scale(vec, 1 / Vec2_length(vec), out);
    }

    void Vec2_normalize(Vec2 &vec) {
        Vec2_normalize(vec, vec);
    }

    void Vec2_print(Vec2 &vec) {
        std::printf("{%f, %f}\n", vec.x, vec.y);
    }
}
