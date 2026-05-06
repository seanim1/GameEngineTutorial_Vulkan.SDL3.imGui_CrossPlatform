add_executable(03_minimal_triangle main.cpp)
target_link_libraries(03_minimal_triangle PRIVATE sdl3_deps vulkan_deps)

find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/vert.spv"
    COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER_DIR}/shader.vert" -o "${SHADER_OUTPUT_DIR}/vert.spv"
    DEPENDS "${SHADER_DIR}/shader.vert"
)

add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/frag.spv"
    COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER_DIR}/shader.frag" -o "${SHADER_OUTPUT_DIR}/frag.spv"
    DEPENDS "${SHADER_DIR}/shader.frag"
)

add_custom_target(03_shaders DEPENDS "${SHADER_OUTPUT_DIR}/vert.spv" "${SHADER_OUTPUT_DIR}/frag.spv")
add_dependencies(03_minimal_triangle 03_shaders)

target_compile_definitions(03_minimal_triangle PRIVATE SHADER_OUTPUT_DIR="${SHADER_OUTPUT_DIR}")