oblo_add_executable(rtsandbox)

target_link_libraries(
    rtsandbox oblo
    CONAN_PKG::sfml
    CONAN_PKG::imgui-sfml
    CONAN_PKG::glew
    CONAN_PKG::assimp
    CONAN_PKG::cxxopts
    CONAN_PKG::cereal)