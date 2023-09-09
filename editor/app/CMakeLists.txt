oblo_add_executable(oblo)

target_link_libraries(
    oblo
    PRIVATE
    oblo::editor::components
    oblo::editor::runtime
    oblo::sandbox
    SDL2::SDL2
    SDL2::SDL2main
)