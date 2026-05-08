#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_main_impl.h>
#include <memory>

struct AppState {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto app = std::make_unique<AppState>();
    *appstate = app.release();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app = std::unique_ptr<AppState>((AppState*)*appstate);
    app->window = SDL_CreateWindow("Part 01 - Window", 800, 600, 0);
    if (!app->window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return SDL_APP_FAILURE;
    }

    app->renderer = SDL_CreateRenderer(app->window, nullptr);
    if (!app->renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        SDL_Quit();
        return SDL_APP_FAILURE;
    }

    *appstate = app.release();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    return (event->type == SDL_EVENT_QUIT) ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    AppState* app = (AppState*)appstate;

    SDL_SetRenderDrawColor(app->renderer, 0, 0, 255, 255);
    SDL_RenderClear(app->renderer);

    SDL_FRect red_rect = {100, 100, 200, 150};
    SDL_SetRenderDrawColor(app->renderer, 255, 0, 0, 255);
    SDL_RenderFillRect(app->renderer, &red_rect);

    SDL_FRect green_rect = {400, 300, 200, 150};
    SDL_SetRenderDrawColor(app->renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(app->renderer, &green_rect);

    SDL_RenderPresent(app->renderer);
    SDL_Delay(16);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto app = std::unique_ptr<AppState>((AppState*)appstate);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_Quit();
}