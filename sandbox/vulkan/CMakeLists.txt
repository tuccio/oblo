oblo_add_executable(vksandbox)

target_link_libraries(
    vksandbox
    PRIVATE
    oblo::core
    oblo::vk
    oblo::vk_renderer
    oblo::sandbox
    imgui::imgui
    SDL2::SDL2
    SDL2::SDL2main
    nlohmann_json::nlohmann_json
)

oblo_create_symlink("${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sandbox/")