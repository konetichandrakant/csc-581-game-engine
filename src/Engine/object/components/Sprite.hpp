#pragma once
#include <string>
#include "../Component.hpp"

namespace Engine::Obj {
    struct Sprite final : Component<Sprite> {
        static constexpr const char* kName = "Sprite";
        std::string textureKey;    // your asset key (e.g., filename or cache id)
        int width{0}, height{0};   // logical size in pixels
        int srcX{0}, srcY{0}, srcW{0}, srcH{0}; // optional atlas rect
        bool visible{true};
    };
}
