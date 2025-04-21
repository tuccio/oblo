# Win32 backend

find_package(ImGui REQUIRED)

add_library(imgui_backend_win32 win32/src/imgui_impl_win32.cpp)
target_include_directories(imgui_backend_win32 PUBLIC win32/include)

target_link_libraries(
    imgui_backend_win32
    imgui::imgui
)

set_target_properties(imgui_backend_win32 PROPERTIES FOLDER ${OBLO_FOLDER_THIRDPARTY})

target_compile_definitions(imgui_backend_win32 INTERFACE "IMGUI_IMPL_API=")
target_compile_definitions(imgui_backend_win32 PRIVATE "IMGUI_IMPL_API=")