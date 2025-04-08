oblo_add_library(luau_rt MODULE)

oblo_register_core_module(luau_rt)

find_package(Luau REQUIRED)

target_link_libraries(
    oblo_luau_rt
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::modules
    oblo::reflection
    oblo::resource
    Luau::Luau
)

add_library(oblo::luau::runtime ALIAS oblo_luau_rt)