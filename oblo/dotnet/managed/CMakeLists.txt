# TODO: Setup the configuration type
add_custom_target(Oblo.Managed-build COMMAND dotnet build \"${CMAKE_CURRENT_SOURCE_DIR}/Oblo.Managed.csproj\" -o \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/managed\")

if(OBLO_GENERATE_CSHARP)
    include_external_msproject(Oblo.Managed "${CMAKE_CURRENT_SOURCE_DIR}/Oblo.Managed.csproj")
endif()