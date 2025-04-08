oblo_add_executable(oblo)

find_package(cxxopts REQUIRED)
find_package(SDL2 REQUIRED)

oblo_register_asset_module_loader(oblo)

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
    SDL2::SDL2
    SDL2::SDL2main
)