oblo_add_executable(cpurtsandbox)

target_link_libraries(
    cpurtsandbox
    PRIVATE
    oblo::core
    oblo::cpurt
    3rdparty::imgui
    3rdparty::imgui_sdl_gl3
    assimp::assimp
    cereal::cereal
    cxxopts::cxxopts
    GLEW::glew_s
    SDL2::SDL2
    SDL2::SDL2main
)