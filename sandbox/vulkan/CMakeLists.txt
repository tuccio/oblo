oblo_add_executable(vksandbox)

target_link_libraries(
    vksandbox
    PRIVATE
    oblo::core
    oblo::vk
    oblo::vk_renderer
    3rdparty::imgui_sdl_vk
    SDL2::SDL2
    SDL2::SDL2main
    nlohmann_json::nlohmann_json
)

oblo_create_symlink("${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sandbox/")