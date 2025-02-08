oblo_add_library(app)

find_package(SDL2 REQUIRED)

target_link_libraries(
    oblo_app
    PUBLIC
    oblo::core
    oblo::modules
    PRIVATE
    SDL2::SDL2
)
