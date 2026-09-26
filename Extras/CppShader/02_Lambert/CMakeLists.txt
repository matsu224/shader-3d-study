cmake_minimum_required(VERSION 3.20)

project(CppShaderLambert LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)

add_executable(cpp_shader_lambert main.cpp)

target_link_libraries(cpp_shader_lambert PRIVATE glfw glm::glm "${OPENGL_FRAMEWORK}")

# 実行場所に依存せずGLSL fileを読めるよう、GLSL directoryの絶対pathをC++へ渡す。
get_filename_component(GLSL_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../GLSL" ABSOLUTE)
target_compile_definitions(
    cpp_shader_lambert
    PRIVATE GLSL_DIRECTORY="${GLSL_DIRECTORY}"
)
