oblo_add_library(modules MODULE)

target_link_libraries(
    oblo_modules
    PUBLIC
    oblo::core
)