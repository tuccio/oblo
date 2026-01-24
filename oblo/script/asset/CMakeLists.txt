oblo_add_library(script_asset MODULE)

oblo_register_asset_module(script_asset)

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

add_library(oblo::script::asset ALIAS oblo_script_asset)