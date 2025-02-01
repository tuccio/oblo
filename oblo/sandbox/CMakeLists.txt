oblo_add_library(sandbox)

find_package(SDL2 REQUIRED)
find_package(iconfontcppheaders REQUIRED)

target_link_libraries(
    oblo_sandbox
    PUBLIC
    oblo::core
    oblo::input
    oblo::vk
    PRIVATE
    oblo::log
    iconfontcppheaders::iconfontcppheaders
    imgui::sdl_vk
    SDL2::SDL2
)