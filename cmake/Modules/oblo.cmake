include(build_configurations)

option(OBLO_ENABLE_ASSERT "Enables internal asserts" OFF)
option(OBLO_DISABLE_COMPILER_OPTIMIZATIONS "Disables compiler optimizations" OFF)
option(OBLO_DEBUG "Activates code useful for debugging" OFF)

define_property(GLOBAL PROPERTY oblo_codegen_config BRIEF_DOCS "Codegen config file" FULL_DOCS "The path to the generated config file used to generate reflection code")

set(OBLO_FOLDER_BUILD "0 - Build")
set(OBLO_FOLDER_APPLICATIONS "1 - Applications")
set(OBLO_FOLDER_LIBRARIES "2 - Libraries")
set(OBLO_FOLDER_TESTS "3 - Tests")
set(OBLO_FOLDER_THIRDPARTY "4 - Third-party")
set(OBLO_FOLDER_CMAKE "5 - CMake")

set(OBLO_CODEGEN_CUSTOM_TARGET run-codegen)

macro(oblo_remove_cxx_flag _option_regex)
    string(TOUPPER ${CMAKE_BUILD_TYPE} _build_type)

    string(REGEX REPLACE "${_option_regex}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "${_option_regex}" "" CMAKE_CXX_FLAGS_${_build_type} "${CMAKE_CXX_FLAGS_${_build_type}}")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_${_build_type} "${CMAKE_CXX_FLAGS_${_build_type}}" PARENT_SCOPE)
endmacro(oblo_remove_cxx_flag)

function(oblo_init_compiler_settings)
    if(MSVC)
        # Warning as errors
        add_compile_options(
            /W4 /WX
            /wd4324 # Padding was added at the end of a structure because you specified an alignment specifier.
            /wd4274 # An exported class was derived from a class that wasn't exported. Disabled because we don't really mix and match DLLs.
            /wd4251 # An exported class was derived from a class that wasn't exported. Disabled because we don't really mix and match DLLs.
        )

        # Disable optimizations if specified
        if(OBLO_DISABLE_COMPILER_OPTIMIZATIONS)
            oblo_remove_cxx_flag("(\/O([a-z])?[0-9a-z])")
            add_compile_options(/Od)
        endif()

        # Enable folders for the solution
        set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    else()
        message(SEND_ERROR "Not supported yet")
    endif()

    if(OBLO_ENABLE_ASSERT)
        add_definitions(-DOBLO_ENABLE_ASSERT)
    endif()

    if(OBLO_DEBUG)
        add_definitions(-DOBLO_DEBUG)
    endif()
endfunction(oblo_init_compiler_settings)

function(oblo_find_source_files)
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
endfunction(oblo_find_source_files)

function(oblo_add_source_files target)
    target_sources(${target} PRIVATE ${_oblo_src} ${_oblo_private_includes} PUBLIC ${_oblo_public_includes})
endfunction(oblo_add_source_files)

function(oblo_add_test_impl name)
    set(_test_target "${_oblo_target_prefix}_test_${name}")

    add_executable(${_test_target} ${_oblo_test_src})
    target_link_libraries(${_test_target} PRIVATE GTest::gtest)

    add_executable("${_oblo_alias_prefix}::test::${name}" ALIAS ${_test_target})
    add_test(NAME ${name} COMMAND ${_test_target} WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

    set_target_properties(
        ${_test_target} PROPERTIES
        FOLDER ${OBLO_FOLDER_TESTS}
        PROJECT_LABEL "${name}_tests"
    )

    target_include_directories(
        ${_test_target} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test>
    )

    set(_oblo_test_target ${_test_target} PARENT_SCOPE)
endfunction(oblo_add_test_impl)

function(oblo_setup_source_groups target)
    source_group("Private\\Source" FILES ${_oblo_src})
    source_group("Private\\Headers" FILES ${_oblo_private_includes})
    source_group("Public\\Headers" FILES ${_oblo_public_includes})
endfunction(oblo_setup_source_groups)

function(oblo_setup_include_dirs target)
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
endfunction(oblo_setup_include_dirs)

macro(_oblo_setup_target_namespace namespace)
    if(NOT DEFINED namespace OR namespace STREQUAL "")
        set(_oblo_alias_prefix "oblo")
        set(_oblo_target_prefix "oblo")
    else()
        set(_oblo_alias_prefix "oblo::${namespace}")
        set(_oblo_target_prefix "oblo_${namespace}")
    endif()
endmacro(_oblo_setup_target_namespace)

function(oblo_add_executable name)
    set(_target "${name}")
    oblo_find_source_files()
    add_executable(${_target})
    oblo_add_source_files(${_target})
    oblo_setup_include_dirs(${_target})
    oblo_setup_source_groups(${_target})
    add_executable("oblo::${name}" ALIAS ${_target})

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
    oblo_find_source_files()

    set(_withReflection FALSE)

    if(_oblo_reflection_includes)
        list(LENGTH _oblo_reflection_includes _reflection_sources_count)

        if(_reflection_sources_count GREATER 1)
            message(FATAL_ERROR "Target ${_target} has ${_reflection_sources_count} reflection include sources, only 1 is supported")
        endif()

        set(_reflection_file ${CMAKE_CURRENT_BINARY_DIR}/${_target}.gen.cpp)
        file(TOUCH ${_reflection_file})
        list(APPEND _oblo_src ${_reflection_file})

        set_property(GLOBAL APPEND PROPERTY oblo_reflection_targets ${_target})

        get_target_property(
            _global_codegen_config
            ${OBLO_CODEGEN_CUSTOM_TARGET}
            oblo_codegen_config
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
            PROPERTIES OBLO_CODEGEN_CONFIG "${_global_reflection_config}"
        )

        set(_withReflection TRUE)
    endif()

    if(NOT DEFINED _oblo_src)
        # Header only library
        add_library(${_target} INTERFACE)

        target_include_directories(
            ${_target} INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )

        target_sources(${_target} INTERFACE ${_oblo_public_includes})

        add_custom_target(${_target}-interface SOURCES ${_oblo_public_includes})
    else()
        # Regular C++ library
        set(_kind "STATIC")

        if(OBLO_LIB_MODULE)
            set(_kind "SHARED")
        endif()

        add_library(${_target} ${_kind})
        oblo_add_source_files(${_target})
        oblo_setup_include_dirs(${_target})

        string(TOUPPER ${name} _upper_name)
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
        endif()

        target_compile_definitions(${_target} PRIVATE "OBLO_PROJECT_NAME=${_target}")
    endif()

    if(DEFINED _oblo_test_src)
        oblo_add_test_impl(${name})
        target_link_libraries(${_oblo_test_target} PRIVATE ${_target})

        if(NOT DEFINED OBLO_LIBRARY_TEST_MAIN)
            target_link_libraries(${_oblo_test_target} PRIVATE GTest::gtest_main)
        endif()
    endif()

    add_library("${_oblo_alias_prefix}::${name}" ALIAS ${_target})
    oblo_setup_source_groups(${_target})

    set_target_properties(
        ${_target} PROPERTIES
        FOLDER ${OBLO_FOLDER_LIBRARIES}
        PROJECT_LABEL ${name}
    )
endfunction(oblo_add_library target)

function(oblo_add_test name)
    cmake_parse_arguments(
        OBLO_TEST
        "MAIN"
        "NAMESPACE"
        ""
        ${ARGN}
    )

    _oblo_setup_target_namespace("${OBLO_TEST_NAMESPACE}")

    oblo_find_source_files()

    if(NOT DEFINED _oblo_test_src)
        message(FATAL_ERROR "Attempting to add a test project '${name}', but no test source files were found")
    endif()

    oblo_add_test_impl(${name})

    if(NOT DEFINED OBLO_TEST_MAIN)
        target_link_libraries(${_oblo_test_target} PRIVATE GTest::gtest_main)
    endif()
endfunction(oblo_add_test name)

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
    file(GENERATE OUTPUT ${_codegen_config_file} CONTENT [\n$<GENEX_EVAL:$<JOIN:$<TARGET_PROPERTY:${OBLO_CODEGEN_CUSTOM_TARGET},OBLO_CODEGEN_CONFIG>,$<COMMA>\n>>\n])

    add_custom_target(${OBLO_CODEGEN_CUSTOM_TARGET} COMMAND $<TARGET_FILE:${_codegen_exe_target}> ${_codegen_config_file})
    set_target_properties(${OBLO_CODEGEN_CUSTOM_TARGET} PROPERTIES oblo_codegen_config "" FOLDER ${OBLO_FOLDER_BUILD})

    set_property(GLOBAL PROPERTY oblo_codegen_config ${_codegen_config_file})
endfunction(oblo_init_reflection)

function(oblo_init)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER ${OBLO_FOLDER_CMAKE})
    oblo_setup_build_configurations()

    add_custom_target(cmake_configure COMMAND ${CMAKE_COMMAND} ${CMAKE_BINARY_DIR})

    set_target_properties(
        cmake_configure PROPERTIES
        FOLDER ${OBLO_FOLDER_BUILD}
        PROJECT_LABEL configure
    )

    oblo_init_reflection()
endfunction(oblo_init)