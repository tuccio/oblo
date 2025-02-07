#pragma once

#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo::vk
{
    class vulkan_engine_module final : public module_interface
    {
    public:
        vulkan_engine_module();
        vulkan_engine_module(const vulkan_engine_module&) = delete;
        vulkan_engine_module(vulkan_engine_module&&) noexcept = delete;
        ~vulkan_engine_module();

        vulkan_engine_module& operator=(const vulkan_engine_module&) = delete;
        vulkan_engine_module& operator=(vulkan_engine_module&&) noexcept = delete;

        bool startup(const module_initializer& initializer) override;
        void shutdown() override;
        void finalize() override;

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}