cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(oblo)

set(_languages CXX)

if(OBLO_GENERATE_CSHARP)
    list(APPEND _languages CSharp)
endif()

project(oblo LANGUAGES ${_languages})

enable_testing()

oblo_init()

find_package(GTest REQUIRED)

# Setup 3rd parties built from source
add_subdirectory(3rdparty)

add_library(imgui::backend_sdl2 ALIAS imgui_backend_sdl2)

# Finally setup our projects
add_subdirectory(oblo)
add_subdirectory(plugins)
add_subdirectory(testing)