# helpful video explaining some basics of cmake:
# https://www.youtube.com/watch?v=7YcbaupsY8I&t=48s

cmake_minimum_required(VERSION 3.10)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)

add_executable(demo src/main.cpp)

target_link_libraries(demo
    SDL3::SDL3
    SDL3_image::SDL3_image
    Engine
)

add_subdirectory(src/Engine)

file(COPY media DESTINATION ${CMAKE_BINARY_DIR})
