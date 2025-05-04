#include <oblo/wasm/wasm_runtime_module.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/modules/utility/registration.hpp>

#include <wasm_export.h>

namespace oblo
{
    struct wasm_runtime_module::impl
    {
        alignas(8) byte wasmHeap[1u << 13];
    };

    wasm_runtime_module::wasm_runtime_module() = default;
    wasm_runtime_module::~wasm_runtime_module() = default;

    bool wasm_runtime_module::startup(const module_initializer&)
    {
        return true;
    }

    bool wasm_runtime_module::finalize()
    {
        m_impl = allocate_unique<impl>();

        RuntimeInitArgs initArgs{};

        initArgs.mem_alloc_type = Alloc_With_Pool;
        initArgs.mem_alloc_option.pool.heap_buf = m_impl->wasmHeap;
        initArgs.mem_alloc_option.pool.heap_size = array_size(m_impl->wasmHeap);

        return wasm_runtime_full_init(&initArgs);
    }

    void wasm_runtime_module::shutdown()
    {
        wasm_runtime_destroy();
    }
}

OBLO_MODULE_REGISTER(oblo::wasm_runtime_module);