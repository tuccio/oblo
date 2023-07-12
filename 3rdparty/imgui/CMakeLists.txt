project(imgui)

add_library(imgui imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp)
target_include_directories(imgui PUBLIC imgui)

# SDL + Vulkan

add_library(imgui_sdl_vk imgui/backends/imgui_impl_sdl.cpp imgui/backends/imgui_impl_vulkan.cpp)
target_include_directories(imgui_sdl_vk PUBLIC imgui/backends)

target_link_libraries(
    imgui_sdl_vk
    imgui
    Vulkan::Headers
    Vulkan::Vulkan
    SDL2::SDL2
)

# SDL + OpenGL3

add_library(imgui_sdl_gl3 imgui/backends/imgui_impl_sdl.cpp imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_sdl_gl3 PUBLIC imgui/backends)

target_link_libraries(
    imgui_sdl_gl3
    imgui
    SDL2::SDL2
)