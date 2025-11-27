#pragma once
#include <string>
#include "../Component.hpp"

namespace Engine::Obj {
    struct Sprite final : Component<Sprite> {
        static constexpr const char* kName = "Sprite";
        std::string textureKey;
        int width{0}, height{0};
        int srcX{0}, srcY{0}, srcW{0}, srcH{0};
        bool visible{true};
    };
}
