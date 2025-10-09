#include "core.h"
#include "entity.h"
#include "physics.h"
#include <SDL3/SDL.h>
#include <vector>
#include <algorithm>
#include <chrono>

namespace Engine {
    SDL_Window * window;
    SDL_Renderer* renderer;
    // TODO: a vector list of entities isn't particularly great for performance, but it works for now
    std::vector<Entity*> entities;
    bool TERMINATE = false;
    int BACKGROUND_COLOR[3] = {0, 32, 128};

    Timeline* timeline;

    void setBackgroundColor(int r, int g, int b) {
        BACKGROUND_COLOR[0] = r;
        BACKGROUND_COLOR[1] = g;
        BACKGROUND_COLOR[2] = b;
    }

    // returns true on success, false on failure
    bool init(const char* windowTitle) {
        // create the SDL window
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            SDL_Log("SDL Init failed: %s", SDL_GetError());
            return false;
        }

        window = SDL_CreateWindow(windowTitle, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
        renderer = SDL_CreateRenderer(window, nullptr);
        timeline = new Timeline();

        if(!SDL_SetRenderVSync(renderer, 1))
            SDL_Log("Vsync not enabled.");

        return true;
    }

    int main(void (*update)(float)) {
        bool running = true;
        TERMINATE = false;
        SDL_Event e;

        while (running) {
            // Event handling
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
            }
            // listen for terminate signal to manually stop the event loop.
            if (TERMINATE) {
                TERMINATE = false; // reset the signal
                running = false; // stop main loop after this iteration
            }

            // compute time since last frame
            timeline->tick();

            //update each entity
            for (auto & e : entities) {
                if (e->hasPhysics())
                    Physics::apply(e, timeline->getDelta());
                e->update(timeline->getDelta());
            }

            // call the user-supplied update function if not nullptr
            if (update) update(timeline->getDelta());


            // Rendering
            SDL_SetRenderDrawColor(renderer,
                BACKGROUND_COLOR[0],
                BACKGROUND_COLOR[1],
                BACKGROUND_COLOR[2],
                255
            );
            SDL_RenderClear(renderer);

            // draw entities
            for (auto & e : entities) {
                e->draw();
            }

            SDL_RenderPresent(renderer);
        }

        // destroy all entities in the list
        while (entities.size() > 0) {
            Entity* e = entities.back();
            delete e;
        }

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }

    bool registerEntity(Engine::Entity *entity) {
        for (auto & e : entities) {
            if (e == entity) return false;
        }
        entities.push_back(entity);
        return true;
    }

    bool unregisterEntity(Entity *entity) {
        entities.erase(std::find(entities.begin(), entities.end(), entity));
        return true;
    }

    void stop() {TERMINATE = true;}
}
