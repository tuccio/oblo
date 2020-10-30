function(oblo_find_source_files)
    file(GLOB_RECURSE _src src/*.cpp)
    file(GLOB_RECURSE _private_includes src/*.hpp)
    file(GLOB_RECURSE _public_includes include/*.hpp)
    file(GLOB_RECURSE _test_src test/*.cpp test/*.hpp)
    
    set(_oblo_src ${_src} PARENT_SCOPE)
    set(_oblo_private_includes ${_private_includes} PARENT_SCOPE)
    set(_oblo_public_includes ${_public_includes} PARENT_SCOPE)
    set(_oblo_test_src ${_test_src} PARENT_SCOPE)
endfunction(oblo_find_source_files)

function(oblo_add_source_files target)
    target_sources(${target} PRIVATE ${_oblo_src} ${_oblo_private_includes} PUBLIC ${_oblo_public_includes})
endfunction(oblo_add_source_files)

function(oblo_setup_source_groups target)
    source_group("Private\\Source" FILES ${_oblo_src})
    source_group("Private\\Headers" FILES ${_oblo_private_includes})
    source_group("Public\\Headers" FILES ${_oblo_public_includes})
endfunction(oblo_setup_source_groups)

function(oblo_setup_include_dirs target)
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

function(oblo_add_executable target)
    oblo_find_source_files()
    add_executable(${target})
    oblo_add_source_files(${target})
    oblo_setup_include_dirs(${target})
    oblo_setup_source_groups(${target})
endfunction(oblo_add_executable target)

function(oblo_add_library target)
    oblo_find_source_files()
    
    if (NOT DEFINED _oblo_src)
        # Header only library
        add_library(${target} INTERFACE)
        
        target_include_directories(
            ${target} INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
        
        target_sources(${target} INTERFACE ${_oblo_public_includes})
        
        add_custom_target(${target}-interface SOURCES ${_oblo_public_includes})
    else ()
        # Regular C++ library
        add_library(${target})
        oblo_add_source_files(${target})
        oblo_setup_include_dirs(${target})
    endif ()

    if (DEFINED _oblo_test_src)
        set(_test_target ${target}_test)
        add_executable(${_test_target} ${_oblo_test_src})
        target_link_libraries(${_test_target} ${target} CONAN_PKG::gtest)
    endif()
    
    oblo_setup_source_groups(${target})
endfunction(oblo_add_library target)

function(oblo_conan_init)
    # Sort of a hack, to disable compiler checks to be able to mix MSVC/clang binaries on windows
    # Related: https://github.com/conan-io/conan/issues/1839
    set(CONAN_DISABLE_CHECK_COMPILER 1)

    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup(TARGETS)
endfunction(oblo_conan_init)

function(oblo_conan_install)
    # Download automatically, you can also just copy the conan.cmake file
    if (NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
        message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
        file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/master/conan.cmake"
            "${CMAKE_BINARY_DIR}/conan.cmake")
    endif()

    include(${CMAKE_BINARY_DIR}/conan.cmake)
    
    # The default profile is to let 3rd party be compiled with msvc on windows
    conan_cmake_run(CONANFILE conanfile.txt PROFILE default PROFILE_AUTO build_type BUILD missing)
endfunction(oblo_conan_install)