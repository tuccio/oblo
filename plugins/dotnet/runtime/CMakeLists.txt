oblo_add_library(
    runtime MODULE
    NAMESPACE oblo::dotnet
)

oblo_register_core_module(oblo::dotnet::runtime)

find_package(dotnet-sdk REQUIRED)
find_package(utf8cpp REQUIRED)

target_link_libraries(
    oblo_dotnet_runtime
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::modules
    oblo::reflection
    oblo::resource
    dotnet-sdk::hostfxr
    utf8cpp
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/oblo.runtimeconfig.json" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dotnet/oblo.runtimeconfig.json" COPYONLY)