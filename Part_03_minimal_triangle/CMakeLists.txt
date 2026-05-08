add_executable(03_minimal_triangle main.cpp)
target_link_libraries(03_minimal_triangle PRIVATE sdl3_deps vulkan_deps)

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

# Copy pre-compiled shaders
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/spv/vert.spv" DESTINATION "${SHADER_OUTPUT_DIR}/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/spv/frag.spv" DESTINATION "${SHADER_OUTPUT_DIR}/")

target_compile_definitions(03_minimal_triangle PRIVATE SHADER_OUTPUT_DIR="${SHADER_OUTPUT_DIR}")