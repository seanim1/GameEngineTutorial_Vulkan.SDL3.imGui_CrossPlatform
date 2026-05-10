#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_main_impl.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <cstring>

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
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    VkRenderPass render_pass = nullptr;
    std::vector<VkFramebuffer> framebuffers;
    VkPipeline pipeline = nullptr;
    VkPipelineLayout pipeline_layout = nullptr;
    VkCommandPool command_pool = nullptr;
    std::vector<VkCommandBuffer> command_buffers;
    VkSemaphore image_available_sem = nullptr;
    VkSemaphore render_finished_sem = nullptr;
    VkFence in_flight_fence = nullptr;
    uint32_t image_count = 0;
    uint32_t graphics_family = 0;
    VkExtent2D swapchain_extent = {};
    VkSurfaceFormatKHR surface_format = {};
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
    /* --- Vulkan API Version --- */
    uint32_t apiVersion = 0;
    vkEnumerateInstanceVersion(&apiVersion);
    SDL_Log("Vulkan API version supported: %d.%d.%d",
        VK_VERSION_MAJOR(apiVersion),
        VK_VERSION_MINOR(apiVersion),
        VK_VERSION_PATCH(apiVersion));

    /* --- Requested Extensions --- */
    std::vector<const char*> requestedInstanceExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
        // On iOS/macOS with MoltenVK, try both Metal surface extensions
        "VK_EXT_metal_surface",
        "VK_MVK_ios_surface",
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
    };

    /* --- Enumerate Supported Extensions --- */
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extCount, NULL);
    std::vector<VkExtensionProperties> supportedInstanceExtensions(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, supportedInstanceExtensions.data());
    SDL_Log("Supported Instance Extensions (%d count):", extCount);
    for (uint32_t i = 0; i < extCount; i++) {
        SDL_Log("\t%d: %s", i, supportedInstanceExtensions[i].extensionName);
    }

    /* --- Filter Enabled Extensions --- */
    std::vector<const char*> enabledInstanceExtensions;
    for (const char* extension : requestedInstanceExtensions) {
        bool found = false;
        for (const auto& supportedExtension : supportedInstanceExtensions) {
            if (strcmp(extension, supportedExtension.extensionName) == 0) {
                enabledInstanceExtensions.push_back(extension);
                SDL_Log("\tInstance Extension '%s' is supported.", extension);
                found = true;
                break;
            }
        }
        if (!found) {
            SDL_Log("\tInstance Extension '%s' is NOT supported.", extension);
        }
    }

    /* --- Enumerate Validation Layers --- */
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    SDL_Log("Supported Validation Layers (%d count):", layerCount);
    for (uint32_t i = 0; i < layerCount; i++) {
        const VkLayerProperties& layer = availableLayers[i];
        SDL_Log("\t%d: %s", i, layer.layerName);
        SDL_Log("\t\tSpec Version: %u", layer.specVersion);
        SDL_Log("\t\tImplementation Version: %u", layer.implementationVersion);
        SDL_Log("\t\tDescription: %s", layer.description);
    }

    /* --- Requested Layers --- */
    std::vector<const char*> requestedInstanceLayers;
    #ifdef _DEBUG
    requestedInstanceLayers.push_back("VK_LAYER_KHRONOS_validation");
    #endif

    /* --- Filter Enabled Layers --- */
    std::vector<const char*> enabledInstanceLayers;
    for (const char* layer : requestedInstanceLayers) {
        bool found = false;
        for (const auto& supportedLayer : availableLayers) {
            if (strcmp(layer, supportedLayer.layerName) == 0) {
                enabledInstanceLayers.push_back(layer);
                SDL_Log("\tInstance Layer '%s' is supported.", layer);
                found = true;
                break;
            }
        }
        if (!found) {
            SDL_Log("\tInstance Layer '%s' is NOT supported.", layer);
        }
    }

    /* --- Create Instance --- */
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Triangle",
        .pEngineName = "Triangle_Engine",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(enabledInstanceLayers.size()),
        .ppEnabledLayerNames = enabledInstanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions.size()),
        .ppEnabledExtensionNames = enabledInstanceExtensions.data(),
    };

#ifdef __APPLE__
    instanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkResult result = vkCreateInstance(&instanceCreateInfo, NULL, &app->instance);
    if (result != VK_SUCCESS) {
        SDL_Log("Failed vkCreateInstance! Error code: %d", result);
        exit(1);
    }
    SDL_Log("Success creating vkCreateInstance");

    /* --- Create Surface (platform agnostic via SDL) --- */
    bool surfaceResult = SDL_Vulkan_CreateSurface(app->window, app->instance, nullptr, &app->surface);
    if (!surfaceResult) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Vulkan_CreateSurface Error: %s", SDL_GetError());
        exit(1);
    }
    SDL_Log("Success creating Vulkan surface via SDL_Vulkan_CreateSurface");

    /* --- Physical Device --- */
    uint32_t device_count = 0;
    check_vk(vkEnumeratePhysicalDevices(app->instance, &device_count, NULL), "enumerate count");
    std::vector<VkPhysicalDevice> devices(device_count);
    check_vk(vkEnumeratePhysicalDevices(app->instance, &device_count, devices.data()), "enumerate devices");
    app->physical_device = devices[0];

    /* --- Queue Family --- */
    uint32_t qfam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device, &qfam_count, NULL);
    std::vector<VkQueueFamilyProperties> qfams(qfam_count);
    vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device, &qfam_count, qfams.data());
    for (uint32_t i = 0; i < qfam_count; i++) {
        if (qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            app->graphics_family = i;
            break;
        }
    }

    /* --- Surface Capabilities --- */
    VkSurfaceCapabilitiesKHR surf_caps;
    check_vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->physical_device, app->surface, &surf_caps), "surface caps");
    app->swapchain_extent = surf_caps.currentExtent;
    app->image_count = surf_caps.minImageCount;
    if ((surf_caps.maxImageCount > 0) && (app->image_count > surf_caps.maxImageCount)) {
        app->image_count = surf_caps.maxImageCount;
    }
    SDL_Log("Surface capabilities - minImageCount: %d", app->image_count);

    /* --- Surface Format Selection --- */
    uint32_t format_count = 0;
    check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, NULL), "format count");
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, formats.data()), "formats");
    SDL_Log("Supported Surface Formats (%d count):", format_count);
    for (uint32_t i = 0; i < format_count; i++) {
        SDL_Log("\t%d: Format: %d, ColorSpace: %d", i, formats[i].format, formats[i].colorSpace);
    }

    app->surface_format = formats[0];
    VkFormat desired_formats[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32
    };

    for (int i = 0; i < 3; i++) {
        for (uint32_t j = 0; j < format_count; j++) {
            if (desired_formats[i] == formats[j].format) {
                app->surface_format = formats[j];
                const char* format_names[] = { "B8G8R8A8_UNORM", "R8G8B8A8_UNORM", "A8B8G8R8_UNORM" };
                SDL_Log("Selected Surface Format: %s", format_names[i]);
                i = 3;
                break;
            }
        }
    }

    /* --- Present Mode Selection --- */
    uint32_t mode_count = 0;
    check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &mode_count, NULL), "mode count");
    std::vector<VkPresentModeKHR> modes(mode_count);
    check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &mode_count, modes.data()), "modes");
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    /* --- Logical Device --- */
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = app->graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
#ifdef __APPLE__
    deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };

    check_vk(vkCreateDevice(app->physical_device, &device_info, NULL, &app->device), "vkCreateDevice");
    vkGetDeviceQueue(app->device, app->graphics_family, 0, &app->graphics_queue);

    /* --- Swapchain --- */
    VkCompositeAlphaFlagBitsKHR alphaMode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surf_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        alphaMode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    } else if (surf_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        alphaMode = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if (surf_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
        alphaMode = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (surf_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (surf_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = app->surface,
        .minImageCount = app->image_count,
        .imageFormat = app->surface_format.format,
        .imageColorSpace = app->surface_format.colorSpace,
        .imageExtent = app->swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = imageUsage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .preTransform = surf_caps.currentTransform,
        .compositeAlpha = alphaMode,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
    };

    check_vk(vkCreateSwapchainKHR(app->device, &swapchain_info, NULL, &app->swapchain), "vkCreateSwapchainKHR");

    /* --- Swapchain Images & Image Views --- */
    check_vk(vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->image_count, NULL), "image count");
    app->images.resize(app->image_count);
    app->image_views.resize(app->image_count);
    check_vk(vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->image_count, app->images.data()), "images");

    for (uint32_t i = 0; i < app->image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = app->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = app->surface_format.format,
            .components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        check_vk(vkCreateImageView(app->device, &view_info, NULL, &app->image_views[i]), "vkCreateImageView");
    }

    SDL_Log("<SwapChain>:");
    SDL_Log("\tSwapchain ImageSize: %u x %u", app->swapchain_extent.width, app->swapchain_extent.height);
    SDL_Log("\tSwapchain ImageCount: %d", app->image_count);
    SDL_Log("\tSwapchain Present Mode: %d", present_mode);

    /* --- Render Pass --- */
    VkAttachmentDescription attachment = {
        .format = app->surface_format.format,
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
    app->framebuffers.resize(app->image_count);
    for (uint32_t i = 0; i < app->image_count; i++) {
        VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = app->render_pass,
            .attachmentCount = 1,
            .pAttachments = &app->image_views[i],
            .width = app->swapchain_extent.width,
            .height = app->swapchain_extent.height,
            .layers = 1,
        };
        check_vk(vkCreateFramebuffer(app->device, &fb_info, NULL, &app->framebuffers[i]), "vkCreateFramebuffer");
    }
#ifdef TARGET_OS_IPHONE
    #define SHADER_OUTPUT_DIR ""
#endif
    /* --- Shaders --- */
    size_t vert_size, frag_size;
    uint32_t* vert_spv = load_shader_file(SHADER_OUTPUT_DIR "vert.spv", &vert_size);
    uint32_t* frag_spv = load_shader_file(SHADER_OUTPUT_DIR "frag.spv", &frag_size);
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

    VkViewport viewport = {.x = 0, .y = 0, .width = (float)app->swapchain_extent.width, .height = (float)app->swapchain_extent.height, .minDepth = 0.0f, .maxDepth = 1.0f};
    VkRect2D scissor = {.offset = {0, 0}, .extent = app->swapchain_extent};
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

    app->command_buffers.resize(app->image_count);
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = app->image_count,
    };
    check_vk(vkAllocateCommandBuffers(app->device, &alloc_info, app->command_buffers.data()), "vkAllocateCommandBuffers");

    /* --- Record Command Buffers --- */
    for (uint32_t i = 0; i < app->image_count; i++) {
        VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check_vk(vkBeginCommandBuffer(app->command_buffers[i], &begin_info), "vkBeginCommandBuffer");

        VkClearValue clear = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo render_pass_begin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = app->render_pass,
            .framebuffer = app->framebuffers[i],
            .renderArea = {{0, 0}, app->swapchain_extent},
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
    for (auto& framebuffer : app->framebuffers) {
        vkDestroyFramebuffer(app->device, framebuffer, NULL);
    }
    for (auto& image_view : app->image_views) {
        vkDestroyImageView(app->device, image_view, NULL);
    }
    vkDestroyRenderPass(app->device, app->render_pass, NULL);
    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
    vkDestroyDevice(app->device, NULL);
    vkDestroySurfaceKHR(app->instance, app->surface, NULL);
    vkDestroyInstance(app->instance, NULL);
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
