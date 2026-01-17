oblo_add_library(script_compiler)

target_link_libraries(
    oblo_script_compiler
    PUBLIC
    oblo::ast
    oblo::script::runtime
)

set(outDir "${CMAKE_CURRENT_BINARY_DIR}/include")
set(generatedHeader "${outDir}/cppgenlib.gen.hpp")
set(sourceFile "${CMAKE_CURRENT_SOURCE_DIR}/resources/cppgenlib.in.hpp")

add_custom_command(
    OUTPUT ${generatedHeader}
    SOURCES ${sourceFile}
    COMMAND ${CMAKE_COMMAND} -DNAME=cppgenlib -DINPUT_FILE=${sourceFile} -DOUTPUT_FILE=${generatedHeader} -P "${CMAKE_SOURCE_DIR}/cmake/Scripts/embed.cmake"
    COMMENT "Embedding ${sourceFile}"
)

target_include_directories(oblo_script_compiler PRIVATE ${outDir})
target_sources(oblo_script_compiler PRIVATE ${generatedHeader})

add_library(oblo::script::compiler ALIAS oblo_script_compiler)