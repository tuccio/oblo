set(_dotnet_bindings_dir "${CMAKE_CURRENT_BINARY_DIR}/dotnet-bindings")

add_custom_target(Oblo.Managed-build COMMAND dotnet build --configuration $<IF:$<CONFIG:Debug>,Debug,Release> \"${CMAKE_CURRENT_SOURCE_DIR}/Oblo.Managed.csproj\" -o \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/managed\" "-p:GeneratedDotNetDir=${_dotnet_bindings_dir}")

set_target_properties(
    Oblo.Managed-build
    PROPERTIES
    OBLO_GENERATED_DOTNET_SOURCE "${_dotnet_bindings_dir}/Bindings.gen.cs"
    PROJECT_LABEL ".NET Build"
)

oblo_set_target_folder(Oblo.Managed-build build)

if(OBLO_GENERATE_CSHARP)
    include_external_msproject(Oblo.Managed "${CMAKE_CURRENT_SOURCE_DIR}/Oblo.Managed.csproj")
    oblo_set_target_folder(Oblo.Managed-build libraries)
endif()

add_dependencies(Oblo.Managed-build run-bindgen)