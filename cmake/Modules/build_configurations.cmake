function(oblo_init_build_configurations)
    get_property(_is_multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

    set(_oblo_cxx_compile_options)
    set(_oblo_cxx_compile_definitions)

    set(_is_msvc FALSE)
    set(_is_clang FALSE)
    set(_is_clangcl FALSE)

    set(_is_x64 FALSE)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "([xX]86_64)|(amd64)|(AMD64)")
        set(_is_x64 TRUE)
    else()
        message(WARNING "Unrecognized target CPU")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(_is_msvc TRUE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        set(_is_clangcl TRUE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(_is_clang TRUE)
    endif()

    if(_is_msvc OR _is_clangcl)
        set(_oblo_cxx_compile_options
            /W4
            /WX
            /wd4324 # Ignore warning C4324: i.e. padding of structs
            /wd4275 # Ignore warning C4275: i.e. dll exported class inheriting from a non-exported one
            /permissive-
        )

        if(_is_x64)
            list(APPEND _oblo_cxx_compile_options
                /arch:AVX2
            )
        endif()

        # Disable optimizations if specified
        if(OBLO_DISABLE_COMPILER_OPTIMIZATIONS)
            _oblo_remove_cxx_flag("(\/O([a-z])?[0-9a-z])")
            list(APPEND _oblo_cxx_compile_options /Od)
        endif()

        if(_is_msvc)
            # Flags that are not supported or needed by clang-cl
            list(APPEND _oblo_cxx_compile_options
                /MP
                /Zc:preprocessor
            )
        endif()
    elseif(_is_clang)
        set(_oblo_cxx_compile_options
            -Werror
            -Wall
        )

        if(_is_x64)
            list(APPEND _oblo_cxx_compile_options
                -march=core-avx2
            )
        endif()
    else()
        message(FATAL_ERROR "Not supported yet")
    endif()

    if(_is_clang OR _is_clangcl)
        list(APPEND _oblo_cxx_compile_options

            # Disable -Wlogical-op-parentheses which warns if you don't add parentheses to A || B && C
            -Wno-logical-op-parentheses

            # Disable -Wmissing-field-initializers, which warns if initializers are missing fields, because unfortunately it triggers with designated initializers too
            -Wno-missing-field-initializers
        )
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

    set_property(GLOBAL PROPERTY oblo_cxx_compile_options "${_oblo_cxx_compile_options}")
    set_property(GLOBAL PROPERTY oblo_cxx_compile_definitions "${_oblo_cxx_compile_definitions}")
endfunction()
