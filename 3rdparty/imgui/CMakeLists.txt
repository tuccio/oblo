find_package(ImGui REQUIRED)

set(imguiDependencies)

if(WIN32)
    set(imguiBackendName win32)
else()
    set(imguiBackendName sdl2)

    find_package(SDL2)
    list(APPEND imguiDependencies SDL2::SDL2)
endif()

add_library(imgui_backend ${imguiBackendName}/src/imgui_impl_${imguiBackendName}.cpp)
target_include_directories(imgui_backend PUBLIC ${imguiBackendName}/include)

target_link_libraries(
    imgui_backend
    PRIVATE
    imgui::imgui
    ${imguiDependencies}
)

set_target_properties(imgui_backend PROPERTIES FOLDER ${OBLO_FOLDER_THIRDPARTY})

target_compile_definitions(imgui_backend INTERFACE "IMGUI_IMPL_API=")
target_compile_definitions(imgui_backend PRIVATE "IMGUI_IMPL_API=")