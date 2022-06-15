oblo_add_executable(rtsandbox)

target_link_libraries(
    rtsandbox
    oblo
    imgui
    imgui_sdl_gl3
    CONAN_PKG::sdl
    CONAN_PKG::glew
    CONAN_PKG::assimp
    CONAN_PKG::cxxopts
    CONAN_PKG::cereal)