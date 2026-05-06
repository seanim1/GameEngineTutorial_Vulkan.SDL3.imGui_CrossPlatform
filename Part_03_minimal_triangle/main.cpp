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
#include <stdbool.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

void check_vk(VkResult result, const char* op) {
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan error in %s: %d\n", op, result);
        exit(1);
    }
}

uint32_t* load_shader_file(const char* filename, size_t* out_size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open shader: %s\n", filename);
        return NULL;
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

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("=== Minimal Vulkan Triangle ===\n\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed\n");
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Vulkan Triangle", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN);
    if (!window) {
        fprintf(stderr, "Window creation failed\n");
        return 1;
    }
    printf("✓ Window created\n");

    /* --- Vulkan Instance --- */
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

    VkInstance instance;
    check_vk(vkCreateInstance(&instance_info, NULL, &instance), "vkCreateInstance");
    printf("✓ Instance created\n");

    /* --- Surface --- */
    VkSurfaceKHR surface = NULL;
#ifdef VK_USE_PLATFORM_WIN32_KHR
    VkWin32SurfaceCreateInfoKHR win32_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandle(NULL),
        .hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL),
    };
    check_vk(vkCreateWin32SurfaceKHR(instance, &win32_info, NULL, &surface), "vkCreateWin32SurfaceKHR");
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    VkXlibSurfaceCreateInfoKHR xlib_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = (Display*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL),
        .window = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0),
    };
    check_vk(vkCreateXlibSurfaceKHR(instance, &xlib_info, NULL, &surface), "vkCreateXlibSurfaceKHR");
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    VkMetalSurfaceCreateInfoEXT metal_info = {
        .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pLayer = (CAMetalLayer*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), "SDL.window.uikit.metal_layer", NULL),
    };
    check_vk(vkCreateMetalSurfaceEXT(instance, &metal_info, NULL, &surface), "vkCreateMetalSurfaceEXT");
#endif
    printf("✓ Surface created\n");

    /* --- Physical Device --- */
    uint32_t device_count = 0;
    check_vk(vkEnumeratePhysicalDevices(instance, &device_count, NULL), "enumerate count");
    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
    check_vk(vkEnumeratePhysicalDevices(instance, &device_count, devices), "enumerate devices");
    VkPhysicalDevice physical_device = devices[0];
    free(devices);
    printf("✓ Physical device selected\n");

    /* --- Queue Family --- */
    uint32_t qfam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qfam_count, NULL);
    VkQueueFamilyProperties* qfams = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * qfam_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qfam_count, qfams);
    uint32_t graphics_family = 0;
    for (uint32_t i = 0; i < qfam_count; i++) {
        if (qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family = i;
            break;
        }
    }
    free(qfams);
    printf("✓ Graphics queue family: %u\n", graphics_family);

    /* --- Surface Capabilities --- */
    VkSurfaceCapabilitiesKHR surface_caps;
    check_vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps), "surface caps");

    uint32_t format_count = 0;
    check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, NULL), "format count");
    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * format_count);
    check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats), "formats");
    VkSurfaceFormatKHR surface_format = formats[0];
    free(formats);

    uint32_t mode_count = 0;
    check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, NULL), "mode count");
    VkPresentModeKHR* modes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * mode_count);
    check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, modes), "modes");
    VkPresentModeKHR present_mode = modes[0];
    free(modes);
    printf("✓ Surface formats queried\n");

    /* --- Logical Device --- */
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphics_family,
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

    VkDevice device;
    check_vk(vkCreateDevice(physical_device, &device_info, NULL, &device), "vkCreateDevice");
    printf("✓ Logical device created\n");

    VkQueue graphics_queue;
    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);

    /* --- Swapchain --- */
    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = 2,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = surface_caps.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surface_caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
    };

    VkSwapchainKHR swapchain;
    check_vk(vkCreateSwapchainKHR(device, &swapchain_info, NULL, &swapchain), "vkCreateSwapchainKHR");
    printf("✓ Swapchain created\n");

    /* --- Swapchain Images & Image Views --- */
    uint32_t image_count = 0;
    check_vk(vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL), "image count");
    VkImage* images = (VkImage*)malloc(sizeof(VkImage) * image_count);
    check_vk(vkGetSwapchainImagesKHR(device, swapchain, &image_count, images), "images");

    VkImageView* image_views = (VkImageView*)malloc(sizeof(VkImageView) * image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface_format.format,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        check_vk(vkCreateImageView(device, &view_info, NULL, &image_views[i]), "vkCreateImageView");
    }
    printf("✓ Image views created\n");

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

    VkRenderPass render_pass;
    check_vk(vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass), "vkCreateRenderPass");
    printf("✓ Render pass created\n");

    /* --- Framebuffers --- */
    VkFramebuffer* framebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = &image_views[i],
            .width = surface_caps.currentExtent.width,
            .height = surface_caps.currentExtent.height,
            .layers = 1,
        };
        check_vk(vkCreateFramebuffer(device, &fb_info, NULL, &framebuffers[i]), "vkCreateFramebuffer");
    }
    printf("✓ Framebuffers created\n");

    /* --- Load & Create Shaders --- */
    size_t vert_size, frag_size;
    uint32_t* vert_spv = load_shader_file(SHADER_OUTPUT_DIR "/vert.spv", &vert_size);
    uint32_t* frag_spv = load_shader_file(SHADER_OUTPUT_DIR "/frag.spv", &frag_size);
    if (!vert_spv || !frag_spv) return 1;

    VkShaderModuleCreateInfo vert_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = vert_size, .pCode = vert_spv};
    VkShaderModuleCreateInfo frag_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = frag_size, .pCode = frag_spv};
    
    VkShaderModule vert_shader, frag_shader;
    check_vk(vkCreateShaderModule(device, &vert_info, NULL, &vert_shader), "vert shader");
    check_vk(vkCreateShaderModule(device, &frag_info, NULL, &frag_shader), "frag shader");
    printf("✓ Shaders loaded\n");

    /* --- Pipeline Layout --- */
    VkPipelineLayoutCreateInfo layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkPipelineLayout pipeline_layout;
    check_vk(vkCreatePipelineLayout(device, &layout_info, NULL, &pipeline_layout), "vkCreatePipelineLayout");

    /* --- Graphics Pipeline --- */
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert_shader, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_shader, .pName = "main"},
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    VkViewport viewport = {.x = 0, .y = 0, .width = (float)surface_caps.currentExtent.width, .height = (float)surface_caps.currentExtent.height, .minDepth = 0.0f, .maxDepth = 1.0f};
    VkRect2D scissor = {.offset = {0, 0}, .extent = surface_caps.currentExtent};
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
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
    };

    VkPipeline pipeline;
    check_vk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline), "vkCreateGraphicsPipelines");
    printf("✓ Graphics pipeline created\n");

    /* --- Command Pool & Buffers --- */
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_family,
    };
    VkCommandPool command_pool;
    check_vk(vkCreateCommandPool(device, &pool_info, NULL, &command_pool), "vkCreateCommandPool");

    VkCommandBuffer* command_buffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * image_count);
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = image_count,
    };
    check_vk(vkAllocateCommandBuffers(device, &alloc_info, command_buffers), "vkAllocateCommandBuffers");

    /* --- Record Command Buffers --- */
    for (uint32_t i = 0; i < image_count; i++) {
        VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check_vk(vkBeginCommandBuffer(command_buffers[i], &begin_info), "vkBeginCommandBuffer");

        VkClearValue clear = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo render_pass_begin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = framebuffers[i],
            .renderArea = {{0, 0}, surface_caps.currentExtent},
            .clearValueCount = 1,
            .pClearValues = &clear,
        };

        vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(command_buffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(command_buffers[i]);

        check_vk(vkEndCommandBuffer(command_buffers[i]), "vkEndCommandBuffer");
    }
    printf("✓ Command buffers recorded\n");

    /* --- Synchronization --- */
    VkSemaphoreCreateInfo sem_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    
    VkSemaphore image_available, render_finished;
    VkFence in_flight;
    check_vk(vkCreateSemaphore(device, &sem_info, NULL, &image_available), "image_available");
    check_vk(vkCreateSemaphore(device, &sem_info, NULL, &render_finished), "render_finished");
    check_vk(vkCreateFence(device, &fence_info, NULL, &in_flight), "in_flight");
    printf("✓ Sync primitives created\n");

    /* --- Main Loop --- */
    printf("\n=== Running (close window to exit) ===\n");
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }

        check_vk(vkWaitForFences(device, 1, &in_flight, VK_TRUE, UINT64_MAX), "vkWaitForFences");
        check_vk(vkResetFences(device, 1, &in_flight), "vkResetFences");

        uint32_t image_index;
        check_vk(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_available, VK_NULL_HANDLE, &image_index), "vkAcquireNextImageKHR");

        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &image_available,
            .pWaitDstStageMask = wait_stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffers[image_index],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &render_finished,
        };
        check_vk(vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight), "vkQueueSubmit");

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_finished,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &image_index,
        };
        check_vk(vkQueuePresentKHR(graphics_queue, &present_info), "vkQueuePresentKHR");

        SDL_Delay(16);
    }

    /* --- Cleanup --- */
    check_vk(vkDeviceWaitIdle(device), "vkDeviceWaitIdle");
    printf("\n✓ Shutting down...\n");

    vkDestroySemaphore(device, image_available, NULL);
    vkDestroySemaphore(device, render_finished, NULL);
    vkDestroyFence(device, in_flight, NULL);
    vkDestroyCommandPool(device, command_pool, NULL);
    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    vkDestroyShaderModule(device, vert_shader, NULL);
    vkDestroyShaderModule(device, frag_shader, NULL);
    for (uint32_t i = 0; i < image_count; i++) {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
        vkDestroyImageView(device, image_views[i], NULL);
    }
    vkDestroyRenderPass(device, render_pass, NULL);
    vkDestroySwapchainKHR(device, swapchain, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);

    free(images);
    free(image_views);
    free(framebuffers);
    free(command_buffers);
    free(vert_spv);
    free(frag_spv);

    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("✓ Done!\n");
    return 0;
}