oblo_add_executable(oblo)

find_package(cxxopts REQUIRED)

oblo_register_asset_module_loader(oblo)
oblo_register_editor_module_loader(oblo)

target_link_libraries(
    oblo
    PRIVATE
    oblo::asset
    oblo::editor::common
    oblo::editor::core
    oblo::editor::windows
    oblo::runtime
    oblo::ecs
    oblo::graphics
    oblo::importers
    oblo::log
    oblo::modules
    oblo::options
    oblo::project
    oblo::app
    oblo::imgui_app
    oblo::scene
    oblo::scene::editor
    oblo::thread
    oblo::vk::engine
    cxxopts::cxxopts
)

oblo_add_data_folder(oblo::editor::data "${CMAKE_SOURCE_DIR}/data/editor")

oblo_symlink_to_data_folder(oblo oblo::editor::data "$<TARGET_FILE_DIR:oblo>/data/oblo")
oblo_symlink_to_data_folder(oblo oblo::imgui::data "$<TARGET_FILE_DIR:oblo>/imgui")
oblo_symlink_to_data_folder(oblo oblo::vulkan::shaders "$<TARGET_FILE_DIR:oblo>/vulkan")

if(OBLO_WITH_DOTNET)
    oblo_symlink_to_data_folder(oblo oblo::dotnet::runtime::data "$<TARGET_FILE_DIR:oblo>/dotnet")
endif()