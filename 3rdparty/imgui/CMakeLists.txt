# SDL2

find_package(ImGui REQUIRED)
find_package(SDL2 REQUIRED)

add_library(imgui_backend_sdl2 sdl2/src/imgui_impl_sdl2.cpp)
target_include_directories(imgui_backend_sdl2 PUBLIC sdl2/include)

target_link_libraries(
    imgui_backend_sdl2
    imgui::imgui
    SDL2::SDL2
)

set_target_properties(imgui_backend_sdl2 PROPERTIES FOLDER ${OBLO_FOLDER_THIRDPARTY})

target_compile_definitions(imgui_backend_sdl2 INTERFACE "IMGUI_IMPL_API=")
target_compile_definitions(imgui_backend_sdl2 PRIVATE "IMGUI_IMPL_API=")