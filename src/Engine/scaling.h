#pragma once

#include <SDL3/SDL_rect.h>

namespace Engine {
    class Scaling {
        private:
        static inline int scalingMode = 0;

        public:
        /*
         * fixed scaling; as the game engine window shrinks and objects move offscreen.
         * this is the default.
         */
        static const int FIXED = 0;

        /*
         * proportional scaling. All objects are guaranteed to stay on screen,
         * but their aspect ratio will change as the aspect ratio of the window changes.
         */
        static const int PROPORTIONAL = 1;

        /*
         * proportional scaling where entities maintain their aspect ratio.
         * content will appear or be truncated along the x axis as window aspect ratio changes.
         * shifted so that the original content remains centered.
         */
        static const int PROPORTIONAL_MAINTAIN_ASPECT_X = 2;

        /*
         * proportional scaling where entities maintain their aspect ratio.
         * content will appear or be truncated along the y axis as window aspect ratio changes.
         * shifted so that the original content remains centered.
         */
         static const int PROPORTIONAL_MAINTAIN_ASPECT_Y = 3;

         /*
          * set the window scaling mode.
          * valid options:
          *     FIXED
          *     PROPORTIONAL
          *     PROPORTIONAL_MAINTAIN_ASPECT_X
          *     PROPORTIONAL_MAINTAIN_ASPECT_Y
          *
          * invalid values will behave like FIXED.
          */
         static void setMode(int mode) {scalingMode = mode;}

         /*
          * get the current window scaling mode.
          */
         static int getMode() {return scalingMode;}

         /*
          * apply the current scaling mode to the given rect.
          * called internally by Entity draw method.
          */
         static SDL_FRect apply(SDL_FRect rect);


         /*
          * given the current window size and scaling mode,
          * returns a struct {x, y, w, h} containing the current visible area.
          * note that PROPORTIONAL_MAINTAIN_ASPECT_X and PROPORTIONAL_MAINTAIN_ASPECT_Y
          * center content, so x and y may be negative.
          */
         static SDL_FRect getVisibleArea();
    };
}
