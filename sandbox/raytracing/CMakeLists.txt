oblo_add_executable(cpurtsandbox)

target_link_libraries(
    cpurtsandbox
    PRIVATE
    oblo::core
    oblo::cpurt
    3rdparty::imgui
    3rdparty::imgui_sdl_gl3
    3rdparty::sdl
    3rdparty::glew
    3rdparty::assimp
    3rdparty::cxxopts
    3rdparty::cereal
)