oblo_add_library(engine MODULE)

target_link_libraries(
    oblo_engine
    PUBLIC
    oblo::asset
    oblo::modules
    oblo::resource
    PRIVATE
    oblo::reflection
)