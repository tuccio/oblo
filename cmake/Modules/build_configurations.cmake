function(oblo_inherit_config_from target base)
    cmake_parse_arguments(
        OBLO_INHERIT
        ""
        "CXX"
        ""
        ${ARGN}
    )

    if(OBLO_INHERIT_CXX)
        set(_cxx ${OBLO_INHERIT_CXX})
    else()
        set(_cxx ${CMAKE_CXX_FLAGS_${base}})
    endif()

    set(CMAKE_C_FLAGS_${target}
        "${CMAKE_C_FLAGS_${base}}"
        CACHE STRING "Flags used by the C compiler during ${target} builds."
        FORCE)

    set(CMAKE_CXX_FLAGS_${target}
        "${_cxx}"
        CACHE STRING "Flags used by the C++ compiler during ${target} builds."
        FORCE)

    set(CMAKE_EXE_LINKER_FLAGS_${target}
        "${CMAKE_EXE_LINKER_FLAGS_${base}}"
        CACHE STRING "Flags used by the exe linker during ${target} builds."
        FORCE)

    set(CMAKE_SHARED_LINKER_FLAGS_${target}
        "${CMAKE_SHARED_LINKER_FLAGS_${base}}"
        CACHE STRING "Flags used by the shared library linker during ${target} builds."
        FORCE)
endfunction()

function(oblo_setup_build_configurations)
    get_property(_is_multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

    if(${_is_multiconfig})
        set(CMAKE_CONFIGURATION_TYPES Debug;Development;Release PARENT_SCOPE)

        if(MSVC)
            # Disable optimizations in development builds
            set(_cxx CXX "/Od /Ob0 /DNDEBUG")
        endif()

        oblo_inherit_config_from(DEVELOPMENT RELWITHDEBINFO ${_cxx})
    endif()

    if(MSVC)
        # Enable folders for the solution to keep the VS solution tidier
        set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    endif()
endfunction()

function(oblo_find_imported_targets var)
    set(targets)
    oblo_find_imported_targets_recursive(targets ${CMAKE_CURRENT_SOURCE_DIR})
    set(${var} ${targets} PARENT_SCOPE)
endfunction()

macro(oblo_find_imported_targets_recursive targets dir)
    get_property(_subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)

    foreach(_subdir ${_subdirectories})
        oblo_find_imported_targets_recursive(${targets} ${_subdir})
    endforeach()

    get_property(_current DIRECTORY ${dir} PROPERTY IMPORTED_TARGETS)
    list(APPEND ${targets} ${_current})
endmacro()

function(oblo_remap_configurations target property)
    get_property(_original TARGET ${target} PROPERTY ${property})

    set(_final)

    foreach(_e ${_original})
        string(REPLACE "$<CONFIG:Release>" "$<OR:$<CONFIG:Release>,$<CONFIG:Development>>" _replaced ${_e})
        list(APPEND _final ${_replaced})
    endforeach()

    set_property(TARGET ${_target} PROPERTY ${property} ${_final})
    message("Original: ${_original}")
    message("Replaced: ${_final}")
endfunction()

function(oblo_finalize_imported_targets)
    oblo_find_imported_targets(_importedTargets)

    foreach(_target ${_importedTargets})
        if(TARGET ${_target})
            oblo_remap_configurations(${_target} INTERFACE_INCLUDE_DIRECTORIES)
            oblo_remap_configurations(${_target} INTERFACE_LINK_DIRECTORIES)
            oblo_remap_configurations(${_target} INTERFACE_LINK_LIBRARIES)
            oblo_remap_configurations(${_target} INTERFACE_COMPILE_DEFINITIONS)
            oblo_remap_configurations(${_target} INTERFACE_COMPILE_OPTIONS)
        endif()
    endforeach()
endfunction()
