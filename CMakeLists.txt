cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(oblo LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/lib)

set(CMAKE_CXX_STANDARD 20)

include(oblo)

oblo_init_compiler_settings()

# Conan packages
oblo_conan_package(gtest/1.10.0)
oblo_conan_package(glew/2.1.0)
oblo_conan_package(assimp/5.0.1)
oblo_conan_package(cxxopts/2.2.1)
oblo_conan_package(cereal/1.3.0)
oblo_conan_package(sdl/2.0.20)
oblo_conan_package(vulkan-headers/1.3.211.0)
oblo_conan_package(vulkan-loader/1.3.211.0)
oblo_conan_package(vulkan-memory-allocator/3.0.0)
oblo_conan_package(glslang/1.3.211.0)
oblo_conan_package(nlohmann_json/3.10.5)

oblo_conan_setup()

# Setup 3rd parties built from source
add_subdirectory(3rdparty)
oblo_3rdparty_create_aliases()

# Finally setup our projects
add_subdirectory(oblo)
add_subdirectory(sandbox)
