

#pragma once
#include "entity.h"
#include "timeline.h"
#include <SDL3/SDL.h>
#include <vector>

namespace Engine {
    /*
     * initial SDL window width, in pixels.
     */
    const int WINDOW_WIDTH = 1920;

    /*
     * initial SDL window height, in pixels.
     */
    const int WINDOW_HEIGHT = 1080;

    /*
     * background color used when clearing the screen.
     * defaults to (0, 32, 128).
     */
    extern int BACKGROUND_COLOR[3];

    /*
     * pointer to the SDL window the game engine is using.
     */
    extern SDL_Window* window;

    /*
     * pointer to the SDL renderer the game engine is using.
     */
    extern SDL_Renderer* renderer;

    /*
     * lists all of the entities in the game/scene (no distinction at the moment)
     */
    extern std::vector<Entity*> entities;

    /*
     * terminate signal. used for ending the main() loop safely, without SDL_Quit occuring.
     */
    extern bool TERMINATE;

    /*
     * default timeline for the game and all game objects.
     */
    extern Timeline* timeline;

    /*
     * sets the background color to the specified (r, g, b) value.
     */
    void setBackgroundColor(int r, int g, int b);

    /*
     * initializes the game engine.
     * initializes SDL, creates the window and renderer instances.
     *
     * returns true on success, and false on failure. will print the error message to stderr on failure.
     */
    bool init(const char* windowTitle);

    /*
     * run the main game loop.
     *
     * calls the user-defined update function once every frame.
     * the signature should be:
     * void update(float dt)
     *
     * where dt is the frame time, in seconds. This is useful for doing time-based physics.
     *
     * automatically handles clearing the screen, listening for SDL_Quit events, and freeing up resources on close.
     * returns an int representing the program status (0 if closed normally)
     */
    int main(void ( *update )(float));

    /*
     * stop (terminate) the main() function. That way, you can close the game from inside the game instead of waiting for the
     * user to close it.
     *
     * NOTE: this will also close the SDL window and free all resources,
     * so you'll have to call init() again if you plan to run another main loop (not sure why you'd want to do that, but still)
     */
     void stop();

    /*
     * add an entity to the global list of entities.
     * meant for internal use
     */
    bool registerEntity(Entity* entity);

     /*
      * remove an entity from the global list of entities.
      * meant for internal use. Called automatically when an entity is destroyed
      */
    bool unregisterEntity(Entity* entity);

    /*
     * Toggle simple status indicators rendered in the main loop.
     */
    void setRecordingIndicatorVisible(bool visible);
    void setPlaybackIndicatorVisible(bool visible);

    /*
     * Optional overlay renderer callback invoked after entities draw, before presenting.
     */
    using OverlayRenderer = void (*)();
    void setOverlayRenderer(OverlayRenderer renderer);

}
