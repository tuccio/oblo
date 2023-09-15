oblo_add_executable(oblo)

target_link_libraries(
    oblo
    PRIVATE
    oblo::editor
    oblo::engine
    oblo::modules
    oblo::sandbox
    oblo::scene
    oblo::importers
    SDL2::SDL2
    SDL2::SDL2main
)