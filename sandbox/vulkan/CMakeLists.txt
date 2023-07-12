oblo_add_executable(vksandbox)

target_link_libraries(
    vksandbox
    PRIVATE
    oblo::core
    oblo::vk
    oblo::render_graph
    3rdparty::imgui_sdl_vk
    SDL2::SDL2
    SDL2::SDL2main
    nlohmann_json::nlohmann_json
)

install(CODE "execute_process( \
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders \
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders \
    )"
)