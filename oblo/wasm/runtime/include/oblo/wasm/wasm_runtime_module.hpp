#pragma once

#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

#include <span>

namespace oblo
{
    class wasm_module;

    class wasm_runtime_module : public module_interface
    {
    public:
        WASM_RT_API wasm_runtime_module();
        wasm_runtime_module(const wasm_runtime_module&) = delete;
        wasm_runtime_module(wasm_runtime_module&&) noexcept = delete;
        WASM_RT_API ~wasm_runtime_module();

        wasm_runtime_module& operator=(const wasm_runtime_module&) = delete;
        wasm_runtime_module& operator=(wasm_runtime_module&&) noexcept = delete;

        WASM_RT_API bool startup(const module_initializer& initializer) override;
        WASM_RT_API bool finalize() override;
        WASM_RT_API void shutdown() override;

        wasm_module load_wasm(std::span<const byte> wasm);

    private:
        struct impl;
        unique_ptr<impl> m_impl;
    };
}