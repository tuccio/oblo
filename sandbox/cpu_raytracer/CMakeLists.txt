oblo_add_executable(cpurtsandbox)

target_link_libraries(
    cpurtsandbox
    PRIVATE
    oblo::core
    oblo::cpurt
    imgui::imgui
    imgui::sdl_gl3
    assimp::assimp
    nlohmann_json::nlohmann_json
    cxxopts::cxxopts
    GLEW::glew_s
    SDL2::SDL2
    SDL2::SDL2main
)