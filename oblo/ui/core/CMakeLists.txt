oblo_add_library(core
    NAMESPACE oblo::ui
)

target_link_libraries(
    oblo_ui_core
    PUBLIC
    oblo::core
)