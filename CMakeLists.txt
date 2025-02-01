cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(oblo LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(oblo)

enable_testing()

oblo_init()

find_package(GTest REQUIRED)

# Setup 3rd parties built from source
add_subdirectory(3rdparty)

add_library(imgui::sdl_vk ALIAS imgui_sdl_vk)

# Finally setup our projects
add_subdirectory(oblo)
add_subdirectory(plugins)
add_subdirectory(testing)