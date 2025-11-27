#pragma once

#include <initializer_list>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace Engine {

    class EventManager;

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

        struct ChordEventInfo {
            std::string chordName;
            float heldDuration;
        };

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

        /*
         * Associate the input system with an EventManager for automatically raised chord events.
         */
        static void setEventManager(EventManager* manager);

        /*
         * Register a chord that becomes active when every listed action is pressed at the same time.
         */
        static void registerChord(const std::string& chordName,
                                  const std::vector<std::string>& actions,
                                  float minHoldTime = 0.0f);

        static void registerChord(const std::string& chordName,
                                  std::initializer_list<std::string> actions,
                                  float minHoldTime = 0.0f);

        /*
         * Query whether a registered chord is currently active.
         */
        static bool chordActive(const std::string& chordName);

        /*
         * Poll and clear any chord events fired since the last update call.
         */
        static std::vector<ChordEventInfo> consumeChordEvents();

        /*
         * Update function that should be called once per frame to evaluate registered chords.
         */
        static void update(float dt);
    };

}
