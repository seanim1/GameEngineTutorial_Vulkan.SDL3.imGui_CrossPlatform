#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_main_impl.h>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Log("=== SDL3 Information ===");
    SDL_Log("SDL Version: %d", SDL_GetVersion());
    SDL_Log("Platform: %s", SDL_GetPlatform());
    SDL_Log("Video Driver: %s", SDL_GetCurrentVideoDriver());

    int num_displays = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&num_displays);
    SDL_Log("Displays: %d", num_displays);
    if (displays) {
        for (int i = 0; i < num_displays; i++) {
            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displays[i]);
            if (mode) {
                SDL_Log("  Display %d: %dx%d @ %.0f Hz", 
                    i, mode->w, mode->h, mode->refresh_rate);
            }
        }
        SDL_free(displays);
    }

    SDL_Log("✓ SDL3 initialized successfully!");
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    return (event->type == SDL_EVENT_QUIT) ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    SDL_Delay(16);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    SDL_Quit();
}