oblo_add_library(game
    NAMESPACE oblo::ui
)

target_link_libraries(
    oblo_ui_game
    PUBLIC
    oblo::core
    oblo::input
    oblo::ui::core
)
