oblo_add_executable(app_sandbox)

find_package(ImGui REQUIRED)

target_link_libraries(
    app_sandbox
    PRIVATE
    oblo::app
    oblo::imgui_app
    oblo::vk_engine
    oblo::log
    oblo::runtime
    imgui::imgui
)
