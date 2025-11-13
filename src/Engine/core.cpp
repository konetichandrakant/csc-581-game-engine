#include "core.h"
#include "entity.h"
#include "physics.h"
#include <SDL3/SDL.h>
#include <vector>
#include <algorithm>
#include <chrono>
#include <iostream>

namespace Engine {
    SDL_Window* window;
    SDL_Renderer* renderer;

    std::vector<Entity*> entities;
    bool TERMINATE = false;
    int BACKGROUND_COLOR[3] = {0, 32, 128};

    Timeline* timeline;
    static bool sShowRecordingIndicator = false;
    static bool sShowPlaybackIndicator = false;
    void (*gameOverlayRenderer)() = nullptr;

    void setBackgroundColor(int r, int g, int b) {
        BACKGROUND_COLOR[0] = r;
        BACKGROUND_COLOR[1] = g;
        BACKGROUND_COLOR[2] = b;
    }
    void setRecordingIndicatorVisible(bool visible) {
        sShowRecordingIndicator = visible;
        if (visible) {
            sShowPlaybackIndicator = false;
        }
    }
    void setPlaybackIndicatorVisible(bool visible) {
        sShowPlaybackIndicator = visible;
        if (visible) {
            sShowRecordingIndicator = false;
        }
    }

    bool init(const char* windowTitle) {

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

            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
            }

            if (TERMINATE) {
                TERMINATE = false;
                running = false;
            }


            timeline->tick();


            for (auto & e : entities) {
                if (e->hasPhysics())
                    Physics::apply(e, timeline->getDelta());
                e->update(timeline->getDelta());
            }


            if (update) update(timeline->getDelta());



            SDL_SetRenderDrawColor(renderer,
                BACKGROUND_COLOR[0],
                BACKGROUND_COLOR[1],
                BACKGROUND_COLOR[2],
                255
            );
            SDL_RenderClear(renderer);


            for (auto & e : entities) {
                e->draw();
            }

            if (sShowRecordingIndicator || sShowPlaybackIndicator) {
                Uint8 r, g, b, a;
                SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

                SDL_FRect dot{12.0f, 12.0f, 18.0f, 18.0f};
                if (sShowRecordingIndicator) {
                    SDL_SetRenderDrawColor(renderer, 220, 20, 60, 255);
                    SDL_RenderFillRect(renderer, &dot);
                } else if (sShowPlaybackIndicator) {
                    SDL_SetRenderDrawColor(renderer, 0, 200, 70, 255);
                    SDL_RenderFillRect(renderer, &dot);
                }

                SDL_SetRenderDrawColor(renderer, r, g, b, a);
            }

            if (gameOverlayRenderer) {
                gameOverlayRenderer();
            }

            SDL_RenderPresent(renderer);

        }


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
