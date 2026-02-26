oblo_add_library(app)

target_link_libraries(
    oblo_app
    PUBLIC
    oblo::core
    oblo::gpu::core
    oblo::input
    oblo::modules
)
