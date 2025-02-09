#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo::vk
{
    class renderer_module final : public module_interface
    {
    public:
        renderer_module();
        renderer_module(const renderer_module&) = delete;
        renderer_module(renderer_module&&) noexcept = delete;
        ~renderer_module();

        renderer_module& operator=(const renderer_module&) = delete;
        renderer_module& operator=(renderer_module&&) noexcept = delete;

        bool startup(const module_initializer& initializer) override;
        void shutdown() override;
        void finalize() override;
    };
}