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

    // some basic vector operations. these do what you think they do!

    // store a + b in out
    void Vec2_add(Vec2 &a, Vec2 &b, Vec2 &out);
    // store a - b in out
    void Vec2_sub(Vec2 &a, Vec2 &b, Vec2 &out);

    // compute length (magnitude) of vec
    float Vec2_length(Vec2 &vec);

    // scale vec, store result in out
    void Vec2_scale(Vec2 &vec, float scale, Vec2 &out);
    // scale vec in-place
    void Vec2_scale(Vec2 &vec, float scale);

    // normalize vec (scale it so it has a magnitude of 1), store result in out
    void Vec2_normalize(Vec2 &vec, Vec2 &out);
    //normalize vec in-place
    void Vec2_normalize(Vec2 &vec);

    // print vector to the console for debugging
    void Vec2_print(Vec2 &vec);
}
