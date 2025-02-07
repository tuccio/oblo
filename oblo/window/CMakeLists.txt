oblo_add_library(window)

find_package(SDL2 REQUIRED)

target_link_libraries(
    oblo_window
    PUBLIC
    oblo::core
    oblo::modules
    PRIVATE
    SDL2::SDL2
)

add_executable(window_sandbox sandbox.cpp)

target_link_libraries(window_sandbox PRIVATE oblo::window oblo::vk_engine oblo::runtime)