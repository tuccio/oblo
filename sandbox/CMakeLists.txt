oblo_add_executable(sandbox)

target_link_libraries(sandbox oblo CONAN_PKG::sfml CONAN_PKG::imgui-sfml CONAN_PKG::glew)