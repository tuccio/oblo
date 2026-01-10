find_package(ImGui REQUIRED)

if(WIN32)
    set(imguiBackendName win32)
else()
    set(imguiBackendName sdl2)
endif()

add_library(imgui_backend ${imguiBackendName}/src/imgui_impl_${imguiBackendName}.cpp)
target_include_directories(imgui_backend PUBLIC ${imguiBackendName}/include)

target_link_libraries(
    imgui_backend
    imgui::imgui
)

set_target_properties(imgui_backend PROPERTIES FOLDER ${OBLO_FOLDER_THIRDPARTY})

target_compile_definitions(imgui_backend INTERFACE "IMGUI_IMPL_API=")
target_compile_definitions(imgui_backend PRIVATE "IMGUI_IMPL_API=")