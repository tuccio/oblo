# SDL + Vulkan

find_package(ImGui REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

add_library(imgui_sdl_vk sdl2/src/imgui_impl_sdl2.cpp vulkan/src/imgui_impl_vulkan.cpp)
target_include_directories(imgui_sdl_vk PUBLIC sdl2/include vulkan/include)

target_link_libraries(
    imgui_sdl_vk
    imgui::imgui
    Vulkan::Headers
    Vulkan::Vulkan
    SDL2::SDL2
)

set_target_properties(imgui_sdl_vk PROPERTIES FOLDER ${OBLO_FOLDER_THIRDPARTY})

target_compile_definitions(imgui_sdl_vk INTERFACE "IMGUI_IMPL_API=")
target_compile_definitions(imgui_sdl_vk PRIVATE "IMGUI_IMPL_API=")