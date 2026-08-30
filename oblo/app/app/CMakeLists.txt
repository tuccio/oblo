oblo_add_library(app)

target_link_libraries(
    oblo_app
    PUBLIC
    oblo::core
    oblo::gpu::core
    oblo::input
    oblo::modules
)

if(LINUX)
    find_package(SDL2 REQUIRED)

    target_link_libraries(
        oblo_app
        PRIVATE
        SDL2::SDL2
    )
endif()
