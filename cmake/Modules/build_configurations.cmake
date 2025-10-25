function(oblo_init_build_configurations)
    get_property(_is_multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

    set(_oblo_cxx_compile_options)
    set(_oblo_cxx_compile_definitions)

    set(_is_msvc FALSE)
    set(_is_clangcl FALSE)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(_is_msvc TRUE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        set(_is_clangcl TRUE)
    endif()

    if(_is_msvc OR _is_clangcl)
        # Warning as errors
        set(_oblo_cxx_compile_options
            /W4
            /WX
            /wd4324 # Ignore warning C4324: i.e. padding of structs
            /wd4275 # Ignore warning C4275: i.e. dll exported class inheriting from a non-exported one
            /permissive-
        )

        # Disable optimizations if specified
        if(OBLO_DISABLE_COMPILER_OPTIMIZATIONS)
            _oblo_remove_cxx_flag("(\/O([a-z])?[0-9a-z])")
            list(APPEND _oblo_cxx_compile_options /Od)
        endif()

        if(_is_msvc)
            # Flags that are note supported or needed by clang-cl
            list(APPEND _oblo_cxx_compile_options
                /MP
                /Zc:preprocessor
            )
        else()
            list(APPEND _oblo_cxx_compile_options

                # Disable -Wlogical-op-parentheses which warns if you don't add parentheses to A || B && C
                -Wno-logical-op-parentheses

                # Disable -Wmissing-field-initializers, which warns if initializers are missing fields, because unfortunately it triggers with designated initializers too
                -Wno-missing-field-initializers
            )
        endif()
    else()
        message(SEND_ERROR "Not supported yet")
    endif()

    if(OBLO_ENABLE_ASSERT)
        list(APPEND _oblo_cxx_compile_definitions "OBLO_ENABLE_ASSERT")
    else()
        list(APPEND _oblo_cxx_compile_definitions "$<$<CONFIG:Debug>:OBLO_ENABLE_ASSERT>")
    endif()

    if(OBLO_DEBUG)
        list(APPEND _oblo_cxx_compile_definitions "OBLO_DEBUG")
    else()
        list(APPEND _oblo_cxx_compile_definitions "$<$<CONFIG:Debug>:OBLO_DEBUG>")
    endif()

    if(${_is_multiconfig})
        set(CMAKE_CONFIGURATION_TYPES Debug;Release PARENT_SCOPE)
    endif()

    set_property(GLOBAL PROPERTY oblo_cxx_compile_options "${_oblo_cxx_compile_options}")
    set_property(GLOBAL PROPERTY oblo_cxx_compile_definitions "${_oblo_cxx_compile_definitions}")
endfunction()
