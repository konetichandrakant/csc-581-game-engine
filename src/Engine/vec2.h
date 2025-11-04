#pragma once

namespace Engine {
    struct Vec2 {
            /*
             * x component of the vector.
             */
            float x = 0;
            /*
             * y component of the vector.
             */
            float y = 0;
    };




    void Vec2_add(Vec2 &a, Vec2 &b, Vec2 &out);

    void Vec2_sub(Vec2 &a, Vec2 &b, Vec2 &out);


    float Vec2_length(Vec2 &vec);


    void Vec2_scale(Vec2 &vec, float scale, Vec2 &out);

    void Vec2_scale(Vec2 &vec, float scale);


    void Vec2_normalize(Vec2 &vec, Vec2 &out);

    void Vec2_normalize(Vec2 &vec);


    void Vec2_print(Vec2 &vec);
}
