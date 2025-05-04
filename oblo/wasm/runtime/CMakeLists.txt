oblo_add_library(wasm_rt MODULE)

oblo_register_core_module(wasm_rt)

find_package(wasm-micro-runtime REQUIRED)

target_link_libraries(
    oblo_wasm_rt
    PUBLIC
    oblo::modules
    PRIVATE
    oblo::log
    wasm-micro-runtime::wasm-micro-runtime
)

target_link_libraries(oblo_test_wasm_rt PRIVATE oblo::test::assets)

add_library(oblo::wasm::runtime ALIAS oblo_wasm_rt)