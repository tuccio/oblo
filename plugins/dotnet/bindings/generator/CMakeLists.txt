oblo_add_executable(
    obindgen
    NAMESPACE oblo::dotnet
)

target_link_libraries(
    obindgen
    PRIVATE
    oblo::ecs
    oblo::graphics
    oblo::modules
    oblo::scene
    oblo::reflection
)