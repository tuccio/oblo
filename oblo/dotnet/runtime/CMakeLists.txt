oblo_add_library(dotnet_rt MODULE)

oblo_register_core_module(dotnet_rt)

find_package(dotnet-sdk REQUIRED)
find_package(utf8cpp REQUIRED)

target_link_libraries(
    oblo_dotnet_rt
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::modules
    oblo::reflection
    oblo::resource
    dotnet-sdk::hostfxr
    utf8cpp
)

add_library(oblo::dotnet::runtime ALIAS oblo_dotnet_rt)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/oblo.runtimeconfig.json" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dotnet/oblo.runtimeconfig.json" COPYONLY)