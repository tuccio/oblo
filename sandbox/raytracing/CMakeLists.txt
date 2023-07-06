oblo_add_executable(rtsandbox)

target_link_libraries(
    rtsandbox
    oblo
    oblort
    3rdparty::imgui
    3rdparty::imgui_sdl_gl3
    3rdparty::sdl
    3rdparty::glew
    3rdparty::assimp
    3rdparty::cxxopts
    3rdparty::cereal
)