# src/Engine/CMakeLists.txt

# Build the Engine library
add_library(Engine
        core.cpp
        vec2.cpp
        entity.cpp
        physics.cpp
        input.cpp
        collision.cpp
        scaling.cpp
        timeline.cpp
        client.cpp
        server.cpp
        Networking.cpp
        JobSystem.cpp
        NetProtocol.hpp
        object/Types.hpp
        object/Component.hpp
        object/GameObject.hpp
        object/GameObject.cpp
        object/Registry.hpp
        object/Registry.cpp
        object/NetworkSceneManager.hpp
        object/NetworkSceneManager.cpp
        object/components/Transform.hpp
        object/components/Sprite.hpp
        object/components/PhysicsBody2D.hpp
        object/components/NetworkPlayer.hpp
)

# Use the KEYWORD signature (fixes the mixed-signature error)
target_link_libraries(Engine
        PUBLIC
        SDL3::SDL3
        SDL3_image::SDL3_image
)

# Make headers available to dependents with "Engine/..." includes.
# Since you include "Engine/Net/Client.h" from outside, expose the project src/ path.
target_include_directories(Engine
        PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)