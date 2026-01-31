oblo_add_executable(
    obindgen
    NAMESPACE oblo::dotnet
)

target_link_libraries(
    obindgen
    PRIVATE
    oblo::dotnet::behaviour
    oblo::ecs
    oblo::graphics
    oblo::modules
    oblo::scene
    oblo::reflection
)

set(_generated_file $<TARGET_PROPERTY:Oblo.Managed-build,OBLO_GENERATED_DOTNET_SOURCE>)
add_custom_target(run-bindgen ALL COMMAND obindgen "${_generated_file}")

oblo_set_target_folder(run-bindgen build)
add_dependencies(run-bindgen run-codegen)

set_target_properties(obindgen PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "${_generated_file}")
