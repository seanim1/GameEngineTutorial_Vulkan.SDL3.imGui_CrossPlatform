cmake_minimum_required(VERSION 3.22)
project(SDL3HelloTriangle VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL3/build")
find_package(SDL3 REQUIRED CONFIG)
find_package(Vulkan REQUIRED)

add_library(sdl3_deps INTERFACE)
target_link_libraries(sdl3_deps INTERFACE SDL3::SDL3)

add_library(vulkan_deps INTERFACE)
target_include_directories(vulkan_deps INTERFACE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(vulkan_deps INTERFACE Vulkan::Vulkan)

# Platform-specific Vulkan defines
if(APPLE)
    target_compile_definitions(vulkan_deps INTERFACE 
        VK_USE_PLATFORM_METAL_EXT
        VK_ENABLE_BETA_EXTENSIONS
    )
elseif(WIN32)
    target_compile_definitions(vulkan_deps INTERFACE VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
    target_compile_definitions(vulkan_deps INTERFACE VK_USE_PLATFORM_XLIB_KHR)
endif()

add_subdirectory(Part_00_init_SDL3)
add_subdirectory(Part_01_window)
add_subdirectory(Part_02_vk_instance)
add_subdirectory(Part_03_minimal_triangle)