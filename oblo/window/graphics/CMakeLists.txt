oblo_add_library(window_graphics)

find_package(SDL2 REQUIRED)

target_link_libraries(
    oblo_window_graphics
    PUBLIC
    oblo::core
    oblo::modules
    PRIVATE
    SDL2::SDL2
)
