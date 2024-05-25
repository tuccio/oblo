oblo_add_library(renderdoc)

target_link_libraries(
    oblo_renderdoc
    PUBLIC
    oblo::core
    oblo::modules
    renderdoc::api
)