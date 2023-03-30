oblo_add_executable(vksandbox)

target_link_libraries(
    vksandbox
    oblo
    oblovk
    oblorg
    CONAN_PKG::sdl
    imgui_sdl_vk
    CONAN_PKG::nlohmann_json)

install(CODE "execute_process( \
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders \
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders \
    )"
)