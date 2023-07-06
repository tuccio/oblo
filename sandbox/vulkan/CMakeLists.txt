oblo_add_executable(vksandbox)

target_link_libraries(
    vksandbox
    oblo
    oblovk
    oblorg
    3rdparty::sdl
    3rdparty::imgui_sdl_vk
    3rdparty::nlohmann_json)

install(CODE "execute_process( \
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders \
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders \
    )"
)