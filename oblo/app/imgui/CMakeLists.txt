oblo_add_library(imgui_app)

find_package(ImGui REQUIRED)

target_link_libraries(
    oblo_imgui_app
    PUBLIC
    oblo::app
    PRIVATE
    oblo::renderer
    oblo::scene
    oblo::vulkan::engine
    imgui::imgui
    imgui::backend
)

if(LINUX)
    find_package(SDL2 REQUIRED)

    target_link_libraries(
        oblo_imgui_app
        PRIVATE
        SDL2::SDL2
    )
endif()

oblo_add_data_folder(oblo::imgui::data "${CMAKE_CURRENT_SOURCE_DIR}/resources")