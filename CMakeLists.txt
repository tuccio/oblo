cmake_minimum_required(VERSION 4.0 FATAL_ERROR)

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

add_library(imgui::backend ALIAS imgui_backend)

# Finally setup our projects
add_subdirectory(oblo)
add_subdirectory(plugins)
add_subdirectory(testing)