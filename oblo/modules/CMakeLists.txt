oblo_add_library(modules SHARED)

target_link_libraries(
    oblo_modules
    PUBLIC
    oblo::core
)