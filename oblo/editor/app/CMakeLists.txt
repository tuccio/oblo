oblo_add_executable(oblo)

find_package(cxxopts REQUIRED)
find_package(SDL2 REQUIRED)

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
    oblo::sandbox
    oblo::scene
    oblo::scene::editor
    oblo::thread
    cxxopts::cxxopts
    SDL2::SDL2
    SDL2::SDL2main
)