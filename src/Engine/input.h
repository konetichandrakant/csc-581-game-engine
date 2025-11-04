#pragma once

#include <map>
#include <string>
#include <unordered_set>

namespace Engine {
    class Input {
        private:
        /*
         * for input mapping.
         *
         * each string literal is mapped to a vector of possible SDL_Scancodes that correspond to that action.
         * That way, for example, left arrow and 'a' can both correspond to 'left' in-game.
         */
        static inline std::map<std::string , std::unordered_set<int>> inputMap;

        public:

        /*
         * determine whether the key is pressed based on SDL_Scancode value.
         * see https:
         *
         */
        static bool keyPressed(int sdlScancode);

        /*
         * determine whether a key is pressed based on a user-defined action name.
         * see map(), unmap(), clear() for creating and updating user-defined actions.
         */
        static bool keyPressed(std::string actionName);

        /*
         * map an action name (string) to an SDL_Scancode.
         *
         * for example, map("left", SDL_SCANCODE_A) would map the A key to an action called 'left'.
         *
         * Multiple scancodes can be mapped to the same action.
         * see https:
         */
        static void map(std::string actionName, int sdlScancode);

        /*
         * unmap a specific SDL_Scancode from the action.
         */
        static void unmap(std::string actionName, int sdlScancode);

        /*
         * unmap all SDL_Scancodes from the action.
         */
        static void clear(std::string actionName);
    };

}
