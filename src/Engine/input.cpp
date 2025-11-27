#include "input.h"
#include "event_manager.h"
#include "events.h"
#include <SDL3/SDL_keyboard.h>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Engine {

    namespace {
        struct ChordBinding {
            std::string name;
            std::vector<std::string> actions;
            float minHold = 0.0f;
        };

        struct ChordRuntimeState {
            bool pressed = false;
            bool fired = false;
            float held = 0.0f;
        };

        std::vector<ChordBinding> sChordBindings;
        std::unordered_map<std::string, ChordRuntimeState> sChordStates;
        std::vector<Input::ChordEventInfo> sChordQueue;
        EventManager* sChordEventManager = nullptr;
    }

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

    void Input::setEventManager(EventManager* manager) {
        sChordEventManager = manager;
    }

    void Input::registerChord(const std::string& chordName,
                              const std::vector<std::string>& actions,
                              float minHoldTime) {
        if (chordName.empty() || actions.empty()) return;

        auto it = std::find_if(sChordBindings.begin(), sChordBindings.end(),
            [&](const ChordBinding& binding) { return binding.name == chordName; });
        if (it != sChordBindings.end()) {
            it->actions = actions;
            it->minHold = minHoldTime;
        } else {
            sChordBindings.push_back(ChordBinding{chordName, actions, minHoldTime});
        }
    }

    void Input::registerChord(const std::string& chordName,
                              std::initializer_list<std::string> actions,
                              float minHoldTime) {
        registerChord(chordName, std::vector<std::string>(actions), minHoldTime);
    }

    bool Input::chordActive(const std::string& chordName) {
        auto it = sChordStates.find(chordName);
        return it != sChordStates.end() && it->second.pressed;
    }

    std::vector<Input::ChordEventInfo> Input::consumeChordEvents() {
        std::vector<ChordEventInfo> events = sChordQueue;
        sChordQueue.clear();
        return events;
    }

    void Input::update(float dt) {
        if (sChordBindings.empty()) return;

        for (const auto& binding : sChordBindings) {
            bool allPressed = true;
            for (const auto& action : binding.actions) {
                if (!Input::keyPressed(action)) {
                    allPressed = false;
                    break;
                }
            }

            auto& state = sChordStates[binding.name];
            if (allPressed) {
                state.held += dt;
                if (!state.pressed) {
                    state.pressed = true;
                    state.held = dt;
                }
                if (!state.fired && state.held >= binding.minHold) {
                    state.fired = true;
                    Input::ChordEventInfo info{binding.name, state.held};
                    sChordQueue.push_back(info);
                    if (sChordEventManager) {
                        auto evt = std::make_shared<InputChordEvent>(binding.name, state.held);
                        sChordEventManager->raise(evt);
                    }
                }
            } else {
                state.pressed = false;
                state.fired = false;
                state.held = 0.0f;
            }
        }
    }
}
