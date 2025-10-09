# About
This is a 2D game engine written in C++ using SDL3.
It was created as a class project for CSC-581 (Game Engine Foundations) at NC State, Fall 2025.

### Authors
- William Jackson (wjackso@ncsu.edu)
- Chandrakant Koneti (ckoneti@ncsu.edu)
- Vaishnav Puram (vpuram@ncsu.edu)
- Bhuvan Chandra Kurra (bkurra@ncsu.edu)


# Setup

## Building the game

assumes you already have make, cmake, SDL3, and SDL3 image installed correctly.

```bash
mkdir build
cd build
cmake ..
make
```

## Running the game

`media` folder is copied into the build directory, so the game can be run from the project root folder or from build.

in build:
```bash
./demo
```

in root folder:
```bash
./build/demo
```
# Documentation

## API Reference

## Project structure

- `media` folder contains all project assets (images, sounds, fonts, etc.)
- `src` folder contains game's source code.
  - `src/Engine` contains the source code for the engine itself.
- `build` folder contains all build output, including the final executable and a copy of the media folder (once built).

For detailed documentation of every function available in the engine, read the header files in `src/Engine/`. Each definition includes a comment detailing its purpose usage.


# Milestone 1: Game Engine Requirements

## Task 1: Core Graphics Setup

The code for this task can be found in `src/Engine/core.h` and `src/Engine/core.cpp`.

- handled by `Engine::init()`
  - Create the foundational code to initialize SDL3.
  - Write the functions to create a window (1920x1080) and a renderer in SDL3.

- handled by `Engine::main()`
  - Implement the main game loop that clears the screen to a blue color, prepares the scene, and renders it.

## Task 2: Generic Entity System

The code for this task can be found in `src/Engine/entity.h` and `src/Engine/entity.cpp`.

- handled by `Engine::Entity` class:
  - Create a general system or class for "entities" or "game objects."
  - The engine needs the capability to draw and update the position of any given entity. It shouldn't care what the entity is, just that it has a position and a texture/shape to render.

## Task 3: Physics System

The code for this task can be found in `src/Engine/physics.h` and `src/Engine/physics.cpp`, as well as `src/Engine/entity.h` and `src/Engine/entity.cpp`.

actually applying physics to an entity is handled by `Engine::Physics::apply` (which is automatically called on every entity in the main loop).
There are also many helper functions in the entity class for setting which physics apply to a given entity (gravity, friction, max speed),
as well as helper functions for setting position and velocity or applying forces.

- Build a generic physics system.

- Handled by `Engine::Physics::setGravity`
  - This system must include a gravity feature that applies a constant downward acceleration to an object.
  Crucially, the strength of gravity must be configurable (e.g., via a function like Physics.setGravity(float value)) and not hard-coded.

## Task 4: Input Handling System
The code for this task can be found in `src/Engine/input.h` and `src/Engine/input.cpp`.

you can ether use SDL Scancodes directly, using `Engine::Input::keyPressed(SDL_SCANCODE)`, or create an abstract mapping using `Engine::Input::map("name", SDL_SCANCODE)` and then checking for presses using `Engine::Input::keyPressed("name")`.

- handled by `Engine::Input` class.
  - Create a manager or system that reads the keyboard state using SDL_GetKeyboardState.
  - This system should provide a simple, abstract way for the game to check if a key is currently pressed (e.g., a function like Input.isKeyPressed(SDL_SCANCODE_W)).

## Task 5: Collision Detection System

The code for this task can be found in `src/Engine/collision.h` and `src/Engine/collision.cpp`.

-Implemented by `Engine::Collision::check()`:
  - Implement a generic function or system that can detect collisions between two game objects.
  - Using bounding box collision (like with SDL_HasIntersection) is the recommended starting point. The system should be able to take two entities and return a true or false value indicating if they are overlapping.

In addition to bounding box intersection, the engine also provides a function for checking which edge of one entity is colliding with another.
this is very useful for building platformer collisions. see `Engine::Collision::checkEdge()`

## Task 6: Scaling System (581 Required)

The code for this task can be found in `src/Engine/scaling.h` and `src/Engine/scaling.cpp`.

- Integrate logic into your rendering system to handle two different scaling modes.
- Implement a mechanism to toggle between constant size (pixel-based) and proportional (percentage-based) scaling. This toggle should be triggered by a key press, linking to your input system.
