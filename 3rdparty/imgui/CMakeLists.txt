project(imgui)

add_library(imgui imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp)
target_include_directories(imgui PUBLIC imgui)

add_library(imgui_sdl_vk imgui/backends/imgui_impl_sdl.cpp imgui/backends/imgui_impl_vulkan.cpp)
target_include_directories(imgui_sdl_vk PUBLIC imgui/backends)

target_link_libraries(
    imgui_sdl_vk
    imgui
    CONAN_PKG::vulkan-loader
    CONAN_PKG::sdl
)