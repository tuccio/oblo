oblo_add_library(luau_asset MODULE)

oblo_register_asset_module(luau_asset)

find_package(Luau REQUIRED)

target_link_libraries(
    oblo_luau_asset
    PRIVATE
    oblo::asset
    oblo::modules
    oblo::luau::runtime
    Luau::Compiler
)

add_library(oblo::luau::asset ALIAS oblo_luau_asset)