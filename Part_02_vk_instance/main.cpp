#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>
#ifdef __APPLE__
    #define VK_ENABLE_BETA_EXTENSIONS
    #include <vulkan/vulkan_metal.h>
    #include <vulkan/vulkan_beta.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void check_vk(VkResult result, const char* op) {
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan error (%s): %d\n", op, result);
        exit(1);
    }
}

VkInstance create_instance() {
    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_METAL_EXT)
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
    };

    uint32_t ext_count = sizeof(extensions) / sizeof(extensions[0]);

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = ext_count,
        .ppEnabledExtensionNames = extensions,
#ifdef VK_USE_PLATFORM_METAL_EXT
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
#endif
    };

    VkInstance instance;
    check_vk(vkCreateInstance(&create_info, NULL, &instance), "vkCreateInstance");
    printf("✓ Instance created\n");
    return instance;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Minimal Triangle Setup ===\n\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed\n");
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Triangle", 800, 600, SDL_WINDOW_VULKAN
    );
    if (!window) {
        fprintf(stderr, "Window creation failed\n");
        SDL_Quit();
        return 1;
    }

    printf("✓ Window created (800x600)\n");

    VkInstance instance = create_instance();

    // TODO: Physical device selection, surface creation, swapchain, etc.

    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || 
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
            }
        }
        SDL_Delay(16);
    }

    vkDestroyInstance(instance, NULL);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("✓ Cleanup complete\n");

    return 0;
}