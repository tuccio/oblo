oblo_add_library(wasm_rt MODULE)

oblo_register_core_module(wasm_rt)

find_package(wasm-micro-runtime REQUIRED)

target_link_libraries(
    oblo_wasm_rt
    PRIVATE
    oblo::log
    oblo::modules
    wasm-micro-runtime::wasm-micro-runtime
)

add_library(oblo::wasm::runtime ALIAS oblo_wasm_rt)