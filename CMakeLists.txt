cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(oblo LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/lib)

set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O0")
endif()

include(oblo)

oblo_conan_init()
oblo_conan_install()

add_subdirectory(oblo)
add_subdirectory(sandbox)