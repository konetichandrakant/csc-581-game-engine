#include "scaling.h"
#include "core.h"
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_video.h>

namespace Engine {
    SDL_FRect Scaling::apply(SDL_FRect rect) {
        int current_w;
        int current_h;

        if (!SDL_GetWindowSize(window, &current_w, &current_h))
            SDL_Log("Can't get window size: %s", SDL_GetError());

        float x_scaling = (float)current_w / (float)WINDOW_WIDTH;
        float y_scaling = (float)current_h / (float)WINDOW_HEIGHT;

        float x_shift = ((float)current_w - (float)WINDOW_WIDTH * y_scaling) / 2;
        float y_shift = ((float)current_h - (float)WINDOW_HEIGHT * x_scaling) / 2;

        switch (scalingMode) {
            case PROPORTIONAL:
                return {
                    rect.x * x_scaling,
                    rect.y * y_scaling,
                    rect.w * x_scaling,
                    rect.h * y_scaling
                };
            case PROPORTIONAL_MAINTAIN_ASPECT_X:



                return {
                    rect.x * y_scaling + x_shift,
                    rect.y * y_scaling,
                    rect.w * y_scaling,
                    rect.h * y_scaling
                };
            case PROPORTIONAL_MAINTAIN_ASPECT_Y:
            return {
                rect.x * x_scaling,
                rect.y * x_scaling + y_shift,
                rect.w * x_scaling,
                rect.h * x_scaling
            };
            default:
                scalingMode = FIXED;
                return rect;
        }
    }

    SDL_FRect Scaling::getVisibleArea() {
        int current_w;
        int current_h;

        if (!SDL_GetWindowSize(window, &current_w, &current_h))
            SDL_Log("Can't get window size: %s", SDL_GetError());

        float default_aspect_ratio = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
        float aspect_ratio = (float)current_w / (float)current_h;

        float width_scaling = WINDOW_WIDTH * aspect_ratio / default_aspect_ratio;
        float height_scaling = WINDOW_HEIGHT * default_aspect_ratio / aspect_ratio;

        switch (scalingMode) {
            case PROPORTIONAL:
                return {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
            case PROPORTIONAL_MAINTAIN_ASPECT_X:
                return {(WINDOW_WIDTH - width_scaling) / 2, 0, width_scaling, WINDOW_HEIGHT};
            case PROPORTIONAL_MAINTAIN_ASPECT_Y:
                return {0, (WINDOW_HEIGHT - height_scaling) / 2, WINDOW_WIDTH, height_scaling};
            default:
                return {0, 0, (float)current_w, (float)current_h};
        }
    }
}
