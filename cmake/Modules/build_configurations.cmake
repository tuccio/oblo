function(oblo_setup_build_configurations)
    get_property(_is_multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

    if(MSVC)
        # Warning as errors
        add_compile_options(/W4 /WX)

        # Ignore warning C4324: i.e. padding of structs
        add_compile_options(/wd4324)

        add_compile_options(/permissive-)
        add_compile_options(/Zc:preprocessor)

        add_compile_options(/MP)
    endif()

    if(${_is_multiconfig})
        set(CMAKE_CONFIGURATION_TYPES Debug;Release PARENT_SCOPE)

        if(MSVC)
            # Enable folders for the solution
            set_property(GLOBAL PROPERTY USE_FOLDERS ON)

            # Disable optimizations in development builds
            set(_cxx CXX "/Od /Ob0 /DNDEBUG")

            add_compile_definitions("$<$<CONFIG:Debug>:OBLO_ENABLE_ASSERT>")
            add_compile_definitions("$<$<CONFIG:Debug>:OBLO_DEBUG>")
        endif()
    elseif(MSVC)
        # Disable optimizations if specified, only really useful for single config generators
        if(OBLO_DISABLE_COMPILER_OPTIMIZATIONS)
            oblo_remove_cxx_flag("(\/O([a-z])?[0-9a-z])")
            add_compile_options(/Od)
        endif()
    else()
        message(SEND_ERROR "Not supported yet")
    endif()

    if(OBLO_ENABLE_ASSERT)
        add_definitions(-DOBLO_ENABLE_ASSERT)
    endif()

    if(OBLO_DEBUG)
        add_definitions(-DOBLO_DEBUG)
    endif()
endfunction()
