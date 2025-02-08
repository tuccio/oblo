oblo_add_library(window_imgui)

find_package(ImGui REQUIRED)
find_package(SDL2 REQUIRED)

target_link_libraries(
    oblo_window_imgui
    PUBLIC
    oblo::window_graphics
    PRIVATE
    oblo::vk_engine
    imgui::imgui
    imgui::sdl_vk
    SDL2::SDL2
)