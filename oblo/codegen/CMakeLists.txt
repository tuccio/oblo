oblo_add_executable(ocodegen)

find_package(LibClang REQUIRED)
find_package(rapidjson REQUIRED)

target_link_libraries(
    ocodegen
    PRIVATE
    oblo::core
    llvm::libclang
    rapidjson
)

get_property(_reflection_config_file GLOBAL PROPERTY oblo_reflection_config)
set_target_properties(ocodegen PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS \"${_reflection_config_file}\")