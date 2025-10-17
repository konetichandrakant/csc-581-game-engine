cmake_minimum_required(VERSION 3.23)
project(GameEngineDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/demo)

# vcpkg packages (resolved via toolchain & triplet)
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(ZeroMQ CONFIG REQUIRED)   # libzmq
find_package(cppzmq CONFIG REQUIRED)   # header-only

# Engine library
add_subdirectory(src/Engine)

# ------------------ executables ------------------
add_executable(client_main  src/client_main.cpp)
add_executable(server_main  src/server_main.cpp)

target_include_directories(client_main PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party)
target_include_directories(server_main PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party)

# ---- Client: console app; DO NOT link SDL3::SDL3main ----
set_target_properties(client_main PROPERTIES WIN32_EXECUTABLE OFF)
target_compile_definitions(client_main PRIVATE SDL_MAIN_HANDLED)

target_link_libraries(client_main
        PRIVATE
        Engine
        SDL3::SDL3              # <- OK
        SDL3_image::SDL3_image  # <- OK
        cppzmq
        libzmq
)

# ---- Server: console, headless ----
set_target_properties(server_main PROPERTIES WIN32_EXECUTABLE OFF)
target_compile_definitions(server_main PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(server_main PRIVATE Engine cppzmq libzmq)

# ---- copy media next to the client exe ----
add_custom_command(TARGET client_main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/media"
        "$<TARGET_FILE_DIR:client_main>/media")

if (MSVC)
    target_compile_definitions(client_main PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(server_main PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif (MINGW)
    # nothing special needed; WIN32_EXECUTABLE OFF is enough
endif()

add_custom_target(build_both ALL DEPENDS client_main server_main)
