define_property(GLOBAL PROPERTY oblo_conan_packages BRIEF_DOCS "Conan packages" FULL_DOCS "List of conan packages registered by the user")
define_property(GLOBAL PROPERTY oblo_conan_package_options BRIEF_DOCS "Conan package options" FULL_DOCS "List of conan package options")

macro(oblo_conan_create_aliases)
    foreach(_conan_target ${CONAN_TARGETS})
        string(REPLACE "CONAN_PKG::" "3rdparty::" _conan_alias ${_conan_target})
        add_library(${_conan_alias} ALIAS ${_conan_target})
    endforeach()
endmacro()

function(oblo_conan_package package)
    cmake_parse_arguments(
        OBLO_CONAN_PACKAGE
        ""
        ""
        "OPTIONS"
        ${ARGN}
    )

    # Match the package name, regex from https://docs.conan.io/en/1.50/reference/conanfile/attributes.html
    string(REGEX MATCH "^[a-zA-Z0-9_][a-zA-Z0-9_\+\.-]+" _name ${package})

    set_property(GLOBAL APPEND PROPERTY oblo_conan_packages ${package})

    foreach(_option ${OBLO_CONAN_PACKAGE_OPTIONS})
        set_property(GLOBAL APPEND PROPERTY oblo_conan_package_options "${_name}:${_option}")
    endforeach(_option ${OBLO_CONAN_PACKAGE_OPTIONS})
endfunction()

function(oblo_conan_generate_conanfile conanfile)
    get_property(_packages GLOBAL PROPERTY oblo_conan_packages)
    get_property(_package_options GLOBAL PROPERTY oblo_conan_package_options)

    list(SORT _packages)
    list(SORT _package_options)

    set(_lines "[requires]")
    list(APPEND _lines "${_packages}")

    list(APPEND _lines "[options]")
    list(APPEND _lines "${_package_options}")

    list(APPEND _lines "[generators]" "cmake" "cmake_find_package")

    list(JOIN _lines "\n" _content)

    file(WRITE ${conanfile} ${_content})
endfunction()

function(oblo_conan_install conanfile)
    # Download automatically, you can also just copy the conan.cmake file
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
        message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
        file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/master/conan.cmake"
            "${CMAKE_BINARY_DIR}/conan.cmake")
    endif()

    include(${CMAKE_BINARY_DIR}/conan.cmake)

    get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

    if(isMultiConfig)
        set(ARGUMENTS_CONFIGURATION_TYPES "")

        if(CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE AND NOT CONAN_EXPORTED AND NOT ARGUMENTS_BUILD_TYPE)
            set(CONAN_CMAKE_MULTI ON)

            if(NOT ARGUMENTS_CONFIGURATION_TYPES)
                set(ARGUMENTS_CONFIGURATION_TYPES "Release;Debug")
            endif()
        endif()

        if(ARGUMENTS_CONFIGURATION_TYPES)
            set(_configuration_types "CONFIGURATION_TYPES ${ARGUMENTS_CONFIGURATION_TYPES}")
        endif()
    else()
        # We don't really care about debugging 3rd party
        set(_build_type ${CMAKE_BUILD_TYPE})

        if(${_build_type} STREQUAL "RelWithDebInfo")
            set(_build_type "build_type=Release")
        endif()
    endif()

    # Sort of a hack, to disable compiler checks to be able to mix MSVC/clang binaries on windows
    # Related: https://github.com/conan-io/conan/issues/1839
    set(CONAN_DISABLE_CHECK_COMPILER 1)

    # The default profile is to let 3rd party be compiled with msvc on windows
    conan_cmake_run(
        CONANFILE ${conanfile}
        BASIC_SETUP
        CMAKE_TARGETS
        PROFILE default
        PROFILE_AUTO
        "${_build_type}"
        "${_configuration_types}"
        BUILD missing
    )

    oblo_conan_create_aliases()
endfunction(oblo_conan_install)

function(oblo_conan_setup)
    set(_conanfile "${CMAKE_BINARY_DIR}/conan/conanfile.txt")
    oblo_conan_generate_conanfile(${_conanfile})
    oblo_conan_install(${_conanfile})
endfunction(oblo_conan_setup)
