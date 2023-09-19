oblo_add_executable(oblo)

target_link_libraries(
    oblo
    PRIVATE
    oblo::editor
    oblo::engine
    oblo::ecs
    oblo::graphics
    oblo::importers
    oblo::modules
    oblo::sandbox
    oblo::scene
    SDL2::SDL2
    SDL2::SDL2main
)