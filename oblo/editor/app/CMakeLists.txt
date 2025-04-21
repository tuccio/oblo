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
    oblo::vk_engine
    cxxopts::cxxopts
)

file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data)
oblo_create_symlink("${CMAKE_CURRENT_SOURCE_DIR}/resources/oblo-assets" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/oblo")
