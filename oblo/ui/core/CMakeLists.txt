oblo_add_library(core
    NAMESPACE oblo::ui
)

find_package(freetype REQUIRED)

target_link_libraries(
    oblo_ui_core
    PUBLIC
    oblo::core
    oblo::input
    PRIVATE
    Freetype::Freetype
)

target_link_libraries(
    oblo_ui_test_core
    PRIVATE
    Freetype::Freetype
)