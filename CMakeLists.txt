cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(oblo LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(oblo)

find_package(assimp REQUIRED)
find_package(cxxopts REQUIRED)
find_package(efsw REQUIRED)
find_package(ktx REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(glew REQUIRED)
find_package(glslang REQUIRED)
find_package(GTest REQUIRED)
find_package(ImGui REQUIRED)
find_package(SDL2 REQUIRED)
find_package(spirv-cross REQUIRED)
find_package(stb REQUIRED)
find_package(tinygltf REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(vulkan-memory-allocator REQUIRED)

oblo_init_compiler_settings()

# Setup 3rd parties built from source
add_subdirectory(3rdparty)

add_library(imgui::sdl_vk ALIAS imgui_sdl_vk)
add_library(imgui::sdl_gl3 ALIAS imgui_sdl_gl3)

# Finally setup our projects
add_subdirectory(editor)
add_subdirectory(oblo)
add_subdirectory(sandbox)
