#pragma once

#include <oblo/core/types.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/shader_compiler.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/swapchain.hpp>
#include <sandbox/imgui.hpp>

struct SDL_Window;

namespace oblo::vk
{
    class sandbox_app
    {
    public:
        sandbox_app() = default;
        sandbox_app(const sandbox_app&) = delete;
        sandbox_app(sandbox_app&&) noexcept = delete;
        sandbox_app& operator=(const sandbox_app&) = delete;
        sandbox_app& operator=(sandbox_app&&) noexcept = delete;
        ~sandbox_app();

        bool init();
        void run();

        void wait_idle();

    private:
        void load_config();
        bool create_window();
        bool create_engine();
        bool create_swapchain();
        bool create_command_pools();
        bool create_synchronization_objects();
        bool create_shader_modules();
        bool create_graphics_pipeline();

        bool create_vertex_buffers();

        void destroy_graphics_pipeline();

    private:
        struct config
        {
            bool vk_use_validation_layers{false};
        };

    private:
        static constexpr u32 SwapchainImages{2u};

        SDL_Window* m_window;
        VkSurfaceKHR m_surface{nullptr};

        instance m_instance;
        single_queue_engine m_engine;
        allocator m_allocator;

        command_buffer_pool m_pools[SwapchainImages];
        swapchain<SwapchainImages> m_swapchain;

        u32 m_renderWidth;
        u32 m_renderHeight;

        vk::shader_compiler m_shaderCompiler;

        VkSemaphore m_presentSemaphore{nullptr};
        VkSemaphore m_timelineSemaphore{nullptr};
        VkFence m_presentFences[SwapchainImages]{nullptr};

        VkShaderModule m_vertShaderModule{nullptr};
        VkShaderModule m_fragShaderModule{nullptr};
        VkPipelineLayout m_pipelineLayout{nullptr};
        VkPipeline m_graphicsPipeline{nullptr};

        allocator::buffer m_positions{};
        allocator::buffer m_colors{};

        u64 m_frameSemaphoreValues[SwapchainImages] = {0};
        u64 m_currentSemaphoreValue{0};

        imgui m_imgui;

        config m_config{};

        bool m_showImgui{false};
    };
}