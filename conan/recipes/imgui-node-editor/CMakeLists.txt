cmake_minimum_required(VERSION 3.8)

project(imgui-node-editor LANGUAGES CXX)

file(GLOB SOURCE_FILES ${SRC_DIR}/*.cpp)
file(GLOB HEADER_FILES ${SRC_DIR}/*.h)

add_library(${PROJECT_NAME} ${SOURCE_FILES})

find_package(ImGui REQUIRED CONFIG)
target_link_libraries(${PROJECT_NAME} PUBLIC imgui::imgui)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

include(GenerateExportHeader)

generate_export_header(${PROJECT_NAME}
    EXPORT_MACRO_NAME IMGUI_NODE_EDITOR_API
)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)

include(GNUInstallDirs)

install(FILES ${SRC_DIR}/imgui_node_editor.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})