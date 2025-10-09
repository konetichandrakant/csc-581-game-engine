#pragma once

// header file for including the entire game engine at once.
#include "core.h" // IWYU pragma: export
#include "vec2.h" // IWYU pragma: export
#include "entity.h" // IWYU pragma: export
#include "physics.h" // IWYU pragma: export
#include "input.h" // IWYU pragma: export
#include "collision.h" // IWYU pragma: export
#include "scaling.h" // IWYU pragma: export
#include "timeline.h" // IWYU pragma: export

// also export scancodes, so that they're available for use in input systems.
#include <SDL3/SDL_scancode.h> // IWYU pragma: export
