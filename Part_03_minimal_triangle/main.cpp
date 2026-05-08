#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_main_impl.h>
#include <vulkan/vulkan.h>
#include <memory>
#ifdef __APPLE__
    #define VK_ENABLE_BETA_EXTENSIONS
    #include <vulkan/vulkan_metal.h>
    #include <vulkan/vulkan_beta.h>
#endif

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

struct AppState {
    SDL_Window* window = nullptr;
    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;
    VkPhysicalDevice physical_device = nullptr;
    VkDevice device = nullptr;
    VkQueue graphics_queue = nullptr;
    VkSwapchainKHR swapchain = nullptr;
    VkImage* images = nullptr;
    VkImageView* image_views = nullptr;
    VkRenderPass render_pass = nullptr;
    VkFramebuffer* framebuffers = nullptr;
    VkPipeline pipeline = nullptr;
    VkPipelineLayout pipeline_layout = nullptr;
    VkCommandPool command_pool = nullptr;
    VkCommandBuffer* command_buffers = nullptr;
    VkSemaphore image_available_sem = nullptr;
    VkSemaphore render_finished_sem = nullptr;
    VkFence in_flight_fence = nullptr;
    uint32_t image_count = 0;
    uint32_t graphics_family = 0;
    VkSurfaceCapabilitiesKHR surface_caps = {};
};

void check_vk(VkResult result, const char* op) {
    if (result != VK_SUCCESS) {
        SDL_Log("Vulkan error (%s): %d", op, result);
        exit(1);
    }
}

uint32_t* load_shader_file(const char* filename, size_t* out_size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        SDL_Log("Failed to open shader: %s", filename);
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t* data = (uint32_t*)malloc(size);
    fread(data, 1, size, f);
    fclose(f);
    *out_size = size;
    return data;
}

void init_vulkan(AppState* app) {
    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_METAL_EXT)
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
    };

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]),
        .ppEnabledExtensionNames = extensions,
#ifdef VK_USE_PLATFORM_METAL_EXT
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
#endif
    };

    check_vk(vkCreateInstance(&instance_info, NULL, &app->instance), "vkCreateInstance");

    /* --- Surface --- */
#ifdef VK_USE_PLATFORM_WIN32_KHR
    VkWin32SurfaceCreateInfoKHR win32_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandle(NULL),
        .hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(app->window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL),
    };
    check_vk(vkCreateWin32SurfaceKHR(app->instance, &win32_info, NULL, &app->surface), "vkCreateWin32SurfaceKHR");
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    VkXlibSurfaceCreateInfoKHR xlib_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = (Display*)SDL_GetPointerProperty(SDL_GetWindowProperties(app->window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL),
        .window = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(app->window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0),
    };
    check_vk(vkCreateXlibSurfaceKHR(app->instance, &xlib_info, NULL, &app->surface), "vkCreateXlibSurfaceKHR");
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    VkMetalSurfaceCreateInfoEXT metal_info = {
        .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pLayer = (CAMetalLayer*)SDL_GetPointerProperty(SDL_GetWindowProperties(app->window), "SDL.window.uikit.metal_layer", NULL),
    };
    check_vk(vkCreateMetalSurfaceEXT(app->instance, &metal_info, NULL, &app->surface), "vkCreateMetalSurfaceEXT");
#endif

    /* --- Physical Device --- */
    uint32_t device_count = 0;
    check_vk(vkEnumeratePhysicalDevices(app->instance, &device_count, NULL), "enumerate count");
    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
    check_vk(vkEnumeratePhysicalDevices(app->instance, &device_count, devices), "enumerate devices");
    app->physical_device = devices[0];
    free(devices);

    /* --- Queue Family --- */
    uint32_t qfam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device, &qfam_count, NULL);
    VkQueueFamilyProperties* qfams = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * qfam_count);
    vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device, &qfam_count, qfams);
    for (uint32_t i = 0; i < qfam_count; i++) {
        if (qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            app->graphics_family = i;
            break;
        }
    }
    free(qfams);

    /* --- Surface Capabilities --- */
    check_vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->physical_device, app->surface, &app->surface_caps), "surface caps");

    uint32_t format_count = 0;
    check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, NULL), "format count");
    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * format_count);
    check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, formats), "formats");
    VkSurfaceFormatKHR surface_format = formats[0];
    free(formats);

    uint32_t mode_count = 0;
    check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &mode_count, NULL), "mode count");
    VkPresentModeKHR* modes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * mode_count);
    check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &mode_count, modes), "modes");
    VkPresentModeKHR present_mode = modes[0];
    free(modes);

    /* --- Logical Device --- */
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = app->graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    const char* device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = sizeof(device_exts) / sizeof(device_exts[0]),
        .ppEnabledExtensionNames = device_exts,
    };

    check_vk(vkCreateDevice(app->physical_device, &device_info, NULL, &app->device), "vkCreateDevice");
    vkGetDeviceQueue(app->device, app->graphics_family, 0, &app->graphics_queue);

    /* --- Swapchain --- */
    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = app->surface,
        .minImageCount = 2,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = app->surface_caps.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = app->surface_caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
    };

    check_vk(vkCreateSwapchainKHR(app->device, &swapchain_info, NULL, &app->swapchain), "vkCreateSwapchainKHR");

    /* --- Swapchain Images & Image Views --- */
    check_vk(vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->image_count, NULL), "image count");
    app->images = (VkImage*)malloc(sizeof(VkImage) * app->image_count);
    check_vk(vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->image_count, app->images), "images");

    app->image_views = (VkImageView*)malloc(sizeof(VkImageView) * app->image_count);
    for (uint32_t i = 0; i < app->image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = app->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface_format.format,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        check_vk(vkCreateImageView(app->device, &view_info, NULL, &app->image_views[i]), "vkCreateImageView");
    }

    /* --- Render Pass --- */
    VkAttachmentDescription attachment = {
        .format = surface_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_ref = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &color_ref};
    
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    check_vk(vkCreateRenderPass(app->device, &render_pass_info, NULL, &app->render_pass), "vkCreateRenderPass");

    /* --- Framebuffers --- */
    app->framebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * app->image_count);
    for (uint32_t i = 0; i < app->image_count; i++) {
        VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = app->render_pass,
            .attachmentCount = 1,
            .pAttachments = &app->image_views[i],
            .width = app->surface_caps.currentExtent.width,
            .height = app->surface_caps.currentExtent.height,
            .layers = 1,
        };
        check_vk(vkCreateFramebuffer(app->device, &fb_info, NULL, &app->framebuffers[i]), "vkCreateFramebuffer");
    }

    /* --- Shaders --- */
    size_t vert_size, frag_size;
    uint32_t* vert_spv = load_shader_file(SHADER_OUTPUT_DIR "/vert.spv", &vert_size);
    uint32_t* frag_spv = load_shader_file(SHADER_OUTPUT_DIR "/frag.spv", &frag_size);
    if (!vert_spv || !frag_spv) return;

    VkShaderModuleCreateInfo vert_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = vert_size, .pCode = vert_spv};
    VkShaderModuleCreateInfo frag_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = frag_size, .pCode = frag_spv};
    
    VkShaderModule vert_shader, frag_shader;
    check_vk(vkCreateShaderModule(app->device, &vert_info, NULL, &vert_shader), "vert shader");
    check_vk(vkCreateShaderModule(app->device, &frag_info, NULL, &frag_shader), "frag shader");

    /* --- Pipeline Layout --- */
    VkPipelineLayoutCreateInfo layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    check_vk(vkCreatePipelineLayout(app->device, &layout_info, NULL, &app->pipeline_layout), "vkCreatePipelineLayout");

    /* --- Graphics Pipeline --- */
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert_shader, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_shader, .pName = "main"},
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    VkViewport viewport = {.x = 0, .y = 0, .width = (float)app->surface_caps.currentExtent.width, .height = (float)app->surface_caps.currentExtent.height, .minDepth = 0.0f, .maxDepth = 1.0f};
    VkRect2D scissor = {.offset = {0, 0}, .extent = app->surface_caps.currentExtent};
    VkPipelineViewportStateCreateInfo viewport_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor};

    VkPipelineRasterizationStateCreateInfo rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_BACK_BIT, .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1.0f};
    VkPipelineMultisampleStateCreateInfo multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

    VkPipelineColorBlendAttachmentState color_blend_attachment = {.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo color_blending = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &color_blend_attachment};

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &color_blending,
        .layout = app->pipeline_layout,
        .renderPass = app->render_pass,
        .subpass = 0,
    };

    check_vk(vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &app->pipeline), "vkCreateGraphicsPipelines");

    /* --- Command Pool & Buffers --- */
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = app->graphics_family,
    };
    check_vk(vkCreateCommandPool(app->device, &pool_info, NULL, &app->command_pool), "vkCreateCommandPool");

    app->command_buffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * app->image_count);
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = app->image_count,
    };
    check_vk(vkAllocateCommandBuffers(app->device, &alloc_info, app->command_buffers), "vkAllocateCommandBuffers");

    /* --- Record Command Buffers --- */
    for (uint32_t i = 0; i < app->image_count; i++) {
        VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check_vk(vkBeginCommandBuffer(app->command_buffers[i], &begin_info), "vkBeginCommandBuffer");

        VkClearValue clear = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo render_pass_begin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = app->render_pass,
            .framebuffer = app->framebuffers[i],
            .renderArea = {{0, 0}, app->surface_caps.currentExtent},
            .clearValueCount = 1,
            .pClearValues = &clear,
        };

        vkCmdBeginRenderPass(app->command_buffers[i], &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(app->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline);
        vkCmdDraw(app->command_buffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(app->command_buffers[i]);

        check_vk(vkEndCommandBuffer(app->command_buffers[i]), "vkEndCommandBuffer");
    }

    /* --- Synchronization --- */
    VkSemaphoreCreateInfo sem_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    
    check_vk(vkCreateSemaphore(app->device, &sem_info, NULL, &app->image_available_sem), "image_available");
    check_vk(vkCreateSemaphore(app->device, &sem_info, NULL, &app->render_finished_sem), "render_finished");
    check_vk(vkCreateFence(app->device, &fence_info, NULL, &app->in_flight_fence), "in_flight");

    free(vert_spv);
    free(frag_spv);
}

void cleanup_vulkan(AppState* app) {
    vkDeviceWaitIdle(app->device);

    vkDestroySemaphore(app->device, app->image_available_sem, NULL);
    vkDestroySemaphore(app->device, app->render_finished_sem, NULL);
    vkDestroyFence(app->device, app->in_flight_fence, NULL);
    vkDestroyCommandPool(app->device, app->command_pool, NULL);
    vkDestroyPipeline(app->device, app->pipeline, NULL);
    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);
    for (uint32_t i = 0; i < app->image_count; i++) {
        vkDestroyFramebuffer(app->device, app->framebuffers[i], NULL);
        vkDestroyImageView(app->device, app->image_views[i], NULL);
    }
    vkDestroyRenderPass(app->device, app->render_pass, NULL);
    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
    vkDestroyDevice(app->device, NULL);
    vkDestroySurfaceKHR(app->instance, app->surface, NULL);
    vkDestroyInstance(app->instance, NULL);

    free(app->images);
    free(app->image_views);
    free(app->framebuffers);
    free(app->command_buffers);
}

void render_frame(AppState* app) {
    check_vk(vkWaitForFences(app->device, 1, &app->in_flight_fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    check_vk(vkResetFences(app->device, 1, &app->in_flight_fence), "vkResetFences");

    uint32_t image_index;
    check_vk(vkAcquireNextImageKHR(app->device, app->swapchain, UINT64_MAX, app->image_available_sem, VK_NULL_HANDLE, &image_index), "vkAcquireNextImageKHR");

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &app->image_available_sem,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &app->command_buffers[image_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &app->render_finished_sem,
    };
    check_vk(vkQueueSubmit(app->graphics_queue, 1, &submit_info, app->in_flight_fence), "vkQueueSubmit");

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &app->render_finished_sem,
        .swapchainCount = 1,
        .pSwapchains = &app->swapchain,
        .pImageIndices = &image_index,
    };
    check_vk(vkQueuePresentKHR(app->graphics_queue, &present_info), "vkQueuePresentKHR");
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto app = std::make_unique<AppState>();
    *appstate = app.release();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app = std::unique_ptr<AppState>((AppState*)*appstate);
    app->window = SDL_CreateWindow("Part 03 - Vulkan Triangle", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN);
    if (!app->window) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return SDL_APP_FAILURE;
    }

    init_vulkan(app.get());
    *appstate = app.release();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    return (event->type == SDL_EVENT_QUIT) ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    render_frame((AppState*)appstate);
    SDL_Delay(16);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto app = std::unique_ptr<AppState>((AppState*)appstate);
    cleanup_vulkan(app.get());
    SDL_DestroyWindow(app->window);
    SDL_Quit();
}