set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(conan_provider)

include(build_configurations)
include(module_loaders)

option(OBLO_ENABLE_ASSERT "Enables internal asserts" OFF)
option(OBLO_DISABLE_COMPILER_OPTIMIZATIONS "Disables compiler optimizations" OFF)
option(OBLO_SKIP_CODEGEN "Disables the codegen dependencies on project, requiring users to run codegen manually" OFF)
option(OBLO_DEBUG "Activates code useful for debugging" OFF)
option(OBLO_GENERATE_CSHARP "Enables C# projects" OFF)
option(OBLO_WITH_DOTNET "Enables .NET modules" ON)
option(OBLO_CONAN_FORCE_INSTALL "Always runs conan install, regardless of conanfile being modified" OFF)

define_property(GLOBAL PROPERTY oblo_codegen_config BRIEF_DOCS "Codegen config file" FULL_DOCS "The path to the generated config file used to generate reflection code")
define_property(GLOBAL PROPERTY oblo_cxx_compile_options BRIEF_DOCS "C++ compile options for oblo targets")
define_property(GLOBAL PROPERTY oblo_cxx_compile_definitions BRIEF_DOCS "C++ compile definitions for oblo targets")

set(OBLO_FOLDER_BUILD "0 - Build")
set(OBLO_FOLDER_APPLICATIONS "1 - Applications")
set(OBLO_FOLDER_LIBRARIES "2 - Libraries")
set(OBLO_FOLDER_TESTS "3 - Tests")
set(OBLO_FOLDER_THIRDPARTY "4 - Third-party")
set(OBLO_FOLDER_CMAKE "5 - CMake")
set(OBLO_FOLDER_INTERNAL "6 - Internal")

set(OBLO_CODEGEN_CUSTOM_TARGET run-codegen)

macro(_oblo_remove_cxx_flag _option_regex)
    string(TOUPPER ${CMAKE_BUILD_TYPE} _build_type)

    string(REGEX REPLACE "${_option_regex}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "${_option_regex}" "" CMAKE_CXX_FLAGS_${_build_type} "${CMAKE_CXX_FLAGS_${_build_type}}")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_${_build_type} "${CMAKE_CXX_FLAGS_${_build_type}}" PARENT_SCOPE)
endmacro(_oblo_remove_cxx_flag)

function(_oblo_find_source_files)
    file(GLOB_RECURSE _src src/*.cpp)
    file(GLOB_RECURSE _private_includes src/*.hpp)
    file(GLOB_RECURSE _public_includes include/*.hpp)
    file(GLOB_RECURSE _test_src test/*.cpp test/*.hpp)
    file(GLOB_RECURSE _reflection_includes reflection/*.hpp)

    set(_oblo_src ${_src} PARENT_SCOPE)
    set(_oblo_private_includes ${_private_includes} PARENT_SCOPE)
    set(_oblo_public_includes ${_public_includes} PARENT_SCOPE)
    set(_oblo_test_src ${_test_src} PARENT_SCOPE)
    set(_oblo_reflection_includes ${_reflection_includes} PARENT_SCOPE)
    set(_oblo_reflection_src PARENT_SCOPE)

    if(WIN32)
        set(_exclusionRegex "_(linux|posix)\\.cpp$")
    elseif(LINUX)
        set(_exclusionRegex "_win32\\.cpp$")
    endif()

    foreach(_var IN ITEMS _src _test_src)
        set(_excluded "${${_var}}")
        list(FILTER _excluded INCLUDE REGEX "${_exclusionRegex}")

        if(_excluded)
            set_source_files_properties("${_excluded}" PROPERTIES HEADER_FILE_ONLY TRUE)
        endif()
    endforeach()
endfunction(_oblo_find_source_files)

function(_oblo_add_source_files target)
    target_sources(${target} PRIVATE ${_oblo_src} ${_oblo_private_includes} ${_oblo_reflection_includes} ${_oblo_reflection_src} PUBLIC ${_oblo_public_includes})
endfunction(_oblo_add_source_files)

function(_oblo_add_test_impl name subfolder)
    set(_test_target "${_oblo_target_prefix}_test_${name}")

    add_executable(${_test_target} ${_oblo_test_src})
    _oblo_configure_cxx_target(${_test_target})
    target_link_libraries(${_test_target} PRIVATE GTest::gtest)

    add_executable("${_oblo_alias_prefix}::test::${name}" ALIAS ${_test_target})
    add_test(NAME ${name} COMMAND ${_test_target} WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

    set_target_properties(
        ${_test_target} PROPERTIES
        FOLDER "${OBLO_FOLDER_TESTS}/${subfolder}"
        PROJECT_LABEL "${name}_tests"
    )

    target_include_directories(
        ${_test_target} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test>
    )

    set(_oblo_test_target ${_test_target} PARENT_SCOPE)
endfunction(_oblo_add_test_impl)

function(_oblo_setup_source_groups)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
        ${_oblo_src}
        ${_oblo_private_includes}
        ${_oblo_public_includes}
    )

    source_group("reflection" FILES
        ${_oblo_reflection_includes}
        ${_oblo_reflection_src}
    )
endfunction(_oblo_setup_source_groups)

function(_oblo_setup_include_dirs target)
    target_include_directories(
        ${target} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_include_directories(
        ${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_include_directories(
        ${target} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )
endfunction(_oblo_setup_include_dirs)

macro(_oblo_setup_target_namespace namespace)
    if("${namespace}" STREQUAL "")
        set(_oblo_alias_prefix "oblo")
        set(_oblo_target_prefix "oblo")
    else()
        set(_oblo_alias_prefix "${namespace}")
        string(REPLACE "::" "_" _oblo_target_prefix "${_oblo_alias_prefix}")
    endif()
endmacro(_oblo_setup_target_namespace)

function(_oblo_configure_cxx_target target)
    get_property(_oblo_cxx_compile_options GLOBAL PROPERTY oblo_cxx_compile_options)
    get_property(_oblo_cxx_compile_definitions GLOBAL PROPERTY oblo_cxx_compile_definitions)

    cmake_parse_arguments(
        ARG
        "INTERFACE"
        ""
        ""
        ${ARGN}
    )

    if(ARG_INTERFACE)
        target_compile_definitions(${target} INTERFACE ${_oblo_cxx_compile_definitions})
    else()
        target_compile_definitions(${target} PUBLIC ${_oblo_cxx_compile_definitions})
        target_compile_options(${target} PRIVATE ${_oblo_cxx_compile_options})
    endif()

    set_target_properties(${target} PROPERTIES
        POSITION_INDEPENDENT_CODE TRUE
    )
endfunction()

function(_oblo_add_codegen_dependency target)
    if(NOT OBLO_SKIP_CODEGEN)
        add_dependencies(${target} ${OBLO_CODEGEN_CUSTOM_TARGET})
    endif()
endfunction(_oblo_add_codegen_dependency)

function(oblo_add_executable name)
    set(_target "${name}")
    _oblo_find_source_files()
    add_executable(${_target})
    _oblo_add_source_files(${_target})
    _oblo_setup_include_dirs(${_target})
    _oblo_setup_source_groups()
    _oblo_configure_cxx_target(${_target})
    add_executable("oblo::${name}" ALIAS ${_target})
    target_compile_definitions(${_target} PRIVATE "OBLO_PROJECT_NAME=${_target}")

    set_target_properties(${_target} PROPERTIES FOLDER ${OBLO_FOLDER_APPLICATIONS})

    if(MSVC)
        set_target_properties(${_target} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif(MSVC)
endfunction(oblo_add_executable target)

function(oblo_add_library name)
    cmake_parse_arguments(
        OBLO_LIB
        "MODULE;TEST_MAIN"
        "NAMESPACE"
        ""
        ${ARGN}
    )

    _oblo_setup_target_namespace("${OBLO_LIB_NAMESPACE}")

    set(_target "${_oblo_target_prefix}_${name}")
    _oblo_find_source_files()

    set(_folder "${OBLO_FOLDER_LIBRARIES}")
    set(_target_subfolder "${name}")

    if(OBLO_LIB_NAMESPACE MATCHES "^oblo::(.+)$")
        string(REPLACE "::" "/" _target_subfolder "${CMAKE_MATCH_1}")
    endif()

    set(_folder "${OBLO_FOLDER_LIBRARIES}/${_target_subfolder}")
    set(_withReflection FALSE)

    if(_oblo_reflection_includes)
        list(LENGTH _oblo_reflection_includes _reflection_sources_count)

        if(_reflection_sources_count GREATER 1)
            message(FATAL_ERROR "Target ${_target} has ${_reflection_sources_count} reflection include sources, only 1 is supported")
        endif()

        set(_reflection_file ${CMAKE_CURRENT_BINARY_DIR}/${_target}.gen.cpp)
        file(TOUCH ${_reflection_file})
        list(APPEND _oblo_reflection_src ${_reflection_file})

        set_property(GLOBAL APPEND PROPERTY oblo_reflection_targets ${_target})

        get_target_property(
            _global_codegen_config
            ${OBLO_CODEGEN_CUSTOM_TARGET}
            oblo_codegen_config_content
        )

        list(APPEND _global_codegen_config
            "{\n\
\"target\": \"${_target}\",\n\
\"source_file\": \"${_oblo_reflection_includes}\",\n\
\"output_file\": \"${_reflection_file}\",\n\
\"include_directories\": [ $<JOIN:$<REMOVE_DUPLICATES:$<LIST:TRANSFORM,$<TARGET_PROPERTY:${_target},INCLUDE_DIRECTORIES>,REPLACE,(.+),\"-I\\0\">>,$<COMMA>> ],\n\
\"compile_definitions\": [ $<JOIN:$<REMOVE_DUPLICATES:$<LIST:TRANSFORM,$<TARGET_PROPERTY:${_target},COMPILE_DEFINITIONS>,REPLACE,(.+),\"-D\\0\">>,$<COMMA>> ]\n\
}")

        set_target_properties(
            ${OBLO_CODEGEN_CUSTOM_TARGET}
            PROPERTIES oblo_codegen_config_content "${_global_codegen_config}"
        )

        set(_withReflection TRUE)
    endif()

    set(_vs_proj_target ${_target})

    if(NOT DEFINED _oblo_src AND NOT DEFINED _oblo_reflection_src)
        # Header only library
        add_library(${_target} INTERFACE)
        _oblo_configure_cxx_target(${_target} INTERFACE)

        target_include_directories(
            ${_target} INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )

        target_sources(${_target} INTERFACE ${_oblo_public_includes})

        set(_vs_proj_target ${_target}-interface)
        add_custom_target(${_vs_proj_target} SOURCES ${_oblo_public_includes})
    else()
        # Regular C++ library
        set(_kind "STATIC")

        if(OBLO_LIB_MODULE)
            set(_kind "SHARED")
        endif()

        add_library(${_target} ${_kind})
        _oblo_add_source_files(${_target})
        _oblo_setup_include_dirs(${_target})
        _oblo_configure_cxx_target(${_target})

        string(TOUPPER ${_target} _upper_name)
        set(_api_define "${_upper_name}_API")

        if(OBLO_LIB_MODULE)
            if(MSVC)
                target_compile_definitions(${_target} INTERFACE "${_api_define}=__declspec(dllimport)")
                target_compile_definitions(${_target} PRIVATE "${_api_define}=__declspec(dllexport)")
            else()
                target_compile_definitions(${_target} INTERFACE "${_api_define}=")
                target_compile_definitions(${_target} PRIVATE "${_api_define}=")
            endif()
        else()
            target_compile_definitions(${_target} INTERFACE "${_api_define}=")
            target_compile_definitions(${_target} PRIVATE "${_api_define}=")
        endif()

        if(_withReflection)
            target_link_libraries(${_target} PUBLIC oblo::annotations)
            _oblo_add_codegen_dependency(${_target})
        endif()

        target_compile_definitions(${_target} PRIVATE "OBLO_PROJECT_NAME=${_target}")
    endif()

    if(DEFINED _oblo_test_src)
        _oblo_add_test_impl(${name} ${_target_subfolder})
        target_link_libraries(${_oblo_test_target} PRIVATE ${_target})

        if(NOT DEFINED OBLO_LIBRARY_TEST_MAIN)
            target_link_libraries(${_oblo_test_target} PRIVATE GTest::gtest_main)
        endif()
    endif()

    add_library("${_oblo_alias_prefix}::${name}" ALIAS ${_target})
    _oblo_setup_source_groups()

    set_target_properties(
        ${_vs_proj_target} PROPERTIES
        FOLDER ${_folder}
        PROJECT_LABEL ${name}
    )
endfunction(oblo_add_library target)

function(oblo_create_symlink source target)
    if(WIN32)
        # We create a junction on Windows, to avoid admin rights issues
        file(TO_NATIVE_PATH "${source}" _src)
        file(TO_NATIVE_PATH "${target}" _dst)
        execute_process(COMMAND cmd.exe /c mklink /J "${_dst}" "${_src}")
    else()
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${source} ${target})
    endif()
endfunction(oblo_create_symlink)

function(oblo_init_reflection)
    set(_codegen_exe_target ocodegen)
    set(_codegen_config_file ${CMAKE_CURRENT_BINARY_DIR}/reflection_config-$<CONFIG>.json)
    file(GENERATE OUTPUT ${_codegen_config_file} CONTENT [\n$<GENEX_EVAL:$<JOIN:$<TARGET_PROPERTY:${OBLO_CODEGEN_CUSTOM_TARGET},oblo_codegen_config_content>,$<COMMA>\n>>\n])

    add_custom_target(${OBLO_CODEGEN_CUSTOM_TARGET} ALL COMMAND $<TARGET_FILE:${_codegen_exe_target}> ${_codegen_config_file})
    set_target_properties(${OBLO_CODEGEN_CUSTOM_TARGET} PROPERTIES oblo_codegen_config_content "" FOLDER ${OBLO_FOLDER_BUILD})

    set_property(GLOBAL PROPERTY oblo_codegen_config ${_codegen_config_file})
endfunction(oblo_init_reflection)

function(oblo_init_conan)
    if(OBLO_CONAN_FORCE_INSTALL)
        message(STATUS "Conan install will not be skipped due to CMake configuration")
        return()
    endif()

    # Run conan install only if conanfile.py changed
    set(_conanfile "${CMAKE_SOURCE_DIR}/conanfile.py")

    if(NOT EXISTS "${_conanfile}")
        message(FATAL_ERROR "conanfile.py not found at ${_conanfile}")
    endif()

    file(SHA256 "${_conanfile}" _conanfile_hash)

    if("${_conanfile_hash}" STREQUAL "${OBLO_CONAN_LAST_INSTALL_ID}")
        message(STATUS "Conan install skipped: no change detected")
        set_property(GLOBAL PROPERTY CONAN_INSTALL_SUCCESS TRUE)
    else()
        message(STATUS "Conan install required: last hash was ${OBLO_CONAN_LAST_INSTALL_ID}, current is ${_conanfile_hash}")
        set_property(GLOBAL PROPERTY OBLO_CONAN_PENDING_HASH "${_conanfile_hash}")
    endif()
endfunction()

function(oblo_init)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER ${OBLO_FOLDER_CMAKE})

    add_custom_target(cmake_configure COMMAND ${CMAKE_COMMAND} ${CMAKE_BINARY_DIR})

    set_target_properties(
        cmake_configure PROPERTIES
        FOLDER ${OBLO_FOLDER_BUILD}
        PROJECT_LABEL configure
    )

    oblo_init_build_configurations()
    oblo_init_reflection()
    oblo_init_conan()

    get_property(_is_multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

    if(${_is_multiconfig})
        set(CMAKE_CONFIGURATION_TYPES Debug;Release PARENT_SCOPE)
    endif()
endfunction(oblo_init)

function(oblo_shutdown_conan)
    get_property(_success GLOBAL PROPERTY CONAN_INSTALL_SUCCESS)

    if(${_success})
        get_property(_conanfile_hash GLOBAL PROPERTY OBLO_CONAN_PENDING_HASH)

        if(OBLO_CONAN_PENDING_HASH)
            set(OBLO_CONAN_LAST_INSTALL_ID "${_conanfile_hash}" CACHE STRING "The id of the last successful conan install" FORCE)
            mark_as_advanced(OBLO_CONAN_LAST_INSTALL_ID)
        endif()
    endif()
endfunction()

function(oblo_shutdown)
    oblo_shutdown_conan()
endfunction()

function(oblo_set_target_folder target folder)
    string(TOUPPER ${folder} _upper)
    set(_resolved ${OBLO_FOLDER_${_upper}})
    set_target_properties(${target} PROPERTIES FOLDER ${_resolved})
endfunction(oblo_set_target_folder)
