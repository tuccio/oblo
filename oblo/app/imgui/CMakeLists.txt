oblo_add_library(imgui_app)

find_package(ImGui REQUIRED)

target_link_libraries(
    oblo_imgui_app
    PUBLIC
    oblo::app
    PRIVATE
    oblo::scene
    oblo::vk_engine
    oblo::vk_renderer
    imgui::imgui
    imgui::backend_win32
)

oblo_create_symlink("${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imgui")
