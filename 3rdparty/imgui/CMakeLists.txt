# SDL + Vulkan

add_library(imgui_sdl_vk sdl2/src/imgui_impl_sdl2.cpp vulkan/src/imgui_impl_vulkan.cpp)
target_include_directories(imgui_sdl_vk PUBLIC sdl2/include vulkan/include)

target_link_libraries(
    imgui_sdl_vk
    imgui::imgui
    Vulkan::Headers
    Vulkan::Vulkan
    SDL2::SDL2
)

# SDL + OpenGL3
add_library(imgui_sdl_gl3 sdl2/src/imgui_impl_sdl2.cpp opengl3/src/imgui_impl_opengl3.cpp)
target_include_directories(imgui_sdl_gl3 PUBLIC sdl2/include opengl3/include)

target_link_libraries(
    imgui_sdl_gl3
    imgui::imgui
    SDL2::SDL2
)

set_target_properties(imgui_sdl_vk PROPERTIES FOLDER ${OBLO_FOLDER_THIRDPARTY})
set_target_properties(imgui_sdl_gl3 PROPERTIES FOLDER ${OBLO_FOLDER_THIRDPARTY})