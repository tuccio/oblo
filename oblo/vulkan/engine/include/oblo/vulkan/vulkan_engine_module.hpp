#pragma once

#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo::vk
{
    class frame_graph;
    class renderer;
    class vulkan_context;

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
        bool finalize() override;

        vulkan_context& get_vulkan_context();
        renderer& get_renderer();
        frame_graph& get_frame_graph();

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}