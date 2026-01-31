oblo_add_library(
    asset MODULE
    NAMESPACE oblo::dotnet
)

oblo_register_asset_module(oblo::dotnet::asset)

target_link_libraries(
    oblo_dotnet_asset
    PRIVATE
    oblo::asset
    oblo::modules
    oblo::dotnet::behaviour
)