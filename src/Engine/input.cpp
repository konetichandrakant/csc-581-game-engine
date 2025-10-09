#include "input.h"
#include <SDL3/SDL_keyboard.h>

namespace Engine {

    bool Input::keyPressed(int sdlScancode) {
        return SDL_GetKeyboardState(nullptr)[sdlScancode];
    };

    bool Input::keyPressed(std::string actionName) {
        for (int scancode : inputMap[actionName]) {
            if (Input::keyPressed(scancode)) return true;
        }
        return false;
    }

    void Input::map(std::string actionName, int sdlScancode) {
        inputMap[actionName].insert(sdlScancode);
    }

    void Input::unmap(std::string actionName, int sdlScancode) {
        inputMap[actionName].erase(sdlScancode);
    };

    void Input::clear(std::string actionName) {
        inputMap[actionName].clear();
    };
}
