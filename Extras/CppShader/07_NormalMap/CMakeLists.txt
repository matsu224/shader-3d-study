cmake_minimum_required(VERSION 3.20)

project(CppShaderNormalMap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)

add_executable(cpp_shader_normal_map main.cpp)

target_link_libraries(
    cpp_shader_normal_map
    PRIVATE glfw glm::glm PNG::PNG "${OPENGL_FRAMEWORK}"
)

# 実行場所に依存せずGLSLとNormal Mapを読めるよう、絶対pathをC++へ渡す。
get_filename_component(GLSL_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../GLSL" ABSOLUTE)
get_filename_component(
    NORMAL_MAP_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../Assets/Study/Textures/metal_grate_rusty_nor_gl_4k.png"
    ABSOLUTE
)

target_compile_definitions(
    cpp_shader_normal_map
    PRIVATE
        GLSL_DIRECTORY="${GLSL_DIRECTORY}"
        NORMAL_MAP_PATH="${NORMAL_MAP_PATH}"
)
