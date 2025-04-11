oblo_add_executable(obindgen)

target_link_libraries(
    obindgen
    PRIVATE
    oblo::ecs
    oblo::graphics
    oblo::modules
    oblo::scene
    oblo::reflection
)