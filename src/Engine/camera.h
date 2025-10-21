#pragma once
#include <algorithm>

namespace Engine {

struct Camera {
    // top-left world-space offset shown at (0,0) on screen
    float x{0}, y{0};

    // Deadzone in pixels (keeps camera steady until player leaves this box)
    float deadLeft{200}, deadRight{200}, deadTop{150}, deadBottom{150};

    // Full screen size in pixels
    int screenW{1280}, screenH{720};

    // Optional clamps for world extent
    float minX{-1e9f}, maxX{ 1e9f};
    float minY{-1e9f}, maxY{ 1e9f};

    // Follow a target world-space position
    void follow(float targetX, float targetY) {
        float leftEdge   = x + deadLeft;
        float rightEdge  = x + (screenW - deadRight);
        float topEdge    = y + deadTop;
        float bottomEdge = y + (screenH - deadBottom);

        if (targetX < leftEdge)   x += (targetX - leftEdge);
        if (targetX > rightEdge)  x += (targetX - rightEdge);
        if (targetY < topEdge)    y += (targetY - topEdge);
        if (targetY > bottomEdge) y += (targetY - bottomEdge);

        x = std::clamp(x, minX, std::max(minX, maxX - (float)screenW));
        y = std::clamp(y, minY, std::max(minY, maxY - (float)screenH));
    }
};

inline void worldToScreen(const Camera& cam, float wx, float wy, float& sx, float& sy){
    sx = wx - cam.x;
    sy = wy - cam.y;
}

} // namespace Engine
