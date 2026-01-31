oblo_add_library(
    asset MODULE
    NAMESPACE oblo::script
)

oblo_register_asset_module(oblo::script::asset)

target_link_libraries(
    oblo_script_asset
    PUBLIC
    oblo::nodes::common
    oblo::nodes::core
    PRIVATE
    oblo::asset
    oblo::modules
    oblo::options
    oblo::properties
    oblo::runtime
    oblo::script::compiler
    oblo::script::resource
)