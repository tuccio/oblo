add_subdirectory(graphics)
add_subdirectory(imgui)

find_package(ImGui REQUIRED)

add_executable(window_sandbox sandbox.cpp)
target_link_libraries(window_sandbox PRIVATE oblo::window_graphics oblo::window_imgui oblo::vk_engine oblo::log oblo::runtime imgui::imgui)
