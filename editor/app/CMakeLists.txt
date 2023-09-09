oblo_add_executable(oblo)

target_link_libraries(
    oblo
    PRIVATE
    # oblo::editor::components
    # oblo::editor::runtime
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(WIN32)
    target_link_libraries(oblo
        PRIVATE
        # The platform plugin for Windows, we need to link to it since we use statics libs
        Qt6::QWindowsIntegrationPlugin

        # These are dependencies of qwindows
        Dcomp
        Shcore
        Setupapi
        d3d9
    )
endif()