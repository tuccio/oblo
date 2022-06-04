oblo_add_executable(vksandbox)

target_link_libraries(
    vksandbox
    oblo
    oblovk
    CONAN_PKG::sdl)