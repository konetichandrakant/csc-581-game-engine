# src/Engine/CMakeLists.txt

add_library(Engine
        core.cpp
        vec2.cpp
        entity.cpp
        physics.cpp
        input.cpp
        collision.cpp
        scaling.cpp
        timeline.cpp
        client.cpp         # <-- your fixed file
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
        object/components/Transform.hpp
        object/components/Sprite.hpp
        object/components/PhysicsBody2D.hpp
)

target_link_libraries(Engine
        PUBLIC
        SDL3::SDL3
        SDL3_image::SDL3_image
)

# Winsock for Windows builds (ntohs/inet_pton/etc.)
if (WIN32)
    target_link_libraries(Engine PUBLIC ws2_32)
endif()

# Make headers visible as "Engine/..."
target_include_directories(Engine
        PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)
