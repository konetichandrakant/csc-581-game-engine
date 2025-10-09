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
