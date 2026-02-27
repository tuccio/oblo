#pragma once

#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class frame_graph;
    class renderer;

    namespace gpu
    {
        class gpu_instance;
    }
}

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
        bool finalize() override;

        gpu::gpu_instance& get_gpu_instance();

        renderer& get_renderer();
        frame_graph& get_frame_graph();

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}