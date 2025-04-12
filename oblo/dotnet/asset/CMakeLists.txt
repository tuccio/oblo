oblo_add_library(dotnet_asset MODULE)

oblo_register_asset_module(dotnet_asset)

target_link_libraries(
    oblo_dotnet_asset
    PRIVATE
    oblo::asset
    oblo::modules
    oblo::dotnet::behaviour
)

add_library(oblo::dotnet::asset ALIAS oblo_dotnet_asset)
