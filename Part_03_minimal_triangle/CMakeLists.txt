add_executable(03_minimal_triangle main.cpp)
target_link_libraries(03_minimal_triangle PRIVATE sdl3_deps vulkan_deps)

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")

# Copy pre-compiled shaders
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/vert.spv" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/frag.spv" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/shaders/")

# Pass shader directory to code with trailing slash
target_compile_definitions(03_minimal_triangle PRIVATE SHADER_OUTPUT_DIR="${SHADER_OUTPUT_DIR}")