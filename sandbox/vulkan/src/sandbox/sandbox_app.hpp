#pragma once

#include <concepts>

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/swapchain.hpp>
#include <oblo/vulkan/texture.hpp>
#include <sandbox/context.hpp>
#include <sandbox/imgui.hpp>

struct SDL_Window;

namespace oblo::vk
{
    class sandbox_base
    {
    protected:
        sandbox_base() = default;
        sandbox_base(const sandbox_base&) = delete;
        sandbox_base(sandbox_base&&) noexcept = delete;
        sandbox_base& operator=(const sandbox_base&) = delete;
        sandbox_base& operator=(sandbox_base&&) noexcept = delete;
        ~sandbox_base() = default;

        bool init(std::span<const char* const> instanceExtensions,
                  std::span<const char* const> instanceLayers,
                  std::span<const char* const> deviceExtensions,
                  void* deviceFeaturesList,
                  const VkPhysicalDeviceFeatures* physicalDeviceFeatures);

        void shutdown();

        void wait_idle();

        bool poll_events();
        void begin_frame(u64 frameIndex, u32* outImageIndex, u32* outPoolIndex);
        void begin_command_buffers(u32 poolIndex, VkCommandBuffer* outCommandBuffers, u32 count);
        void end_command_buffers(const VkCommandBuffer* commandBuffers, u32 count);
        void submit_and_present(const VkCommandBuffer* commandBuffers,
                                u32 commandBuffersCount,
                                u32 imageIndex,
                                u32 poolIndex,
                                u64 frameIndex);

    private:
        void load_config();
        bool create_window();

        bool create_engine(std::span<const char* const> instanceExtensions,
                           std::span<const char* const> instanceLayers,
                           std::span<const char* const> deviceExtensions,
                           void* deviceFeaturesList,
                           const VkPhysicalDeviceFeatures* physicalDeviceFeatures);

        bool create_swapchain();
        bool create_command_pools();
        bool create_synchronization_objects();
        bool init_imgui();

        void destroy_swapchain();

    protected:
        struct config
        {
            bool vk_use_validation_layers{false};
        };

    protected:
        static constexpr u32 SwapchainImages{2u};
        static constexpr VkFormat SwapchainFormat{VK_FORMAT_B8G8R8A8_UNORM};

        SDL_Window* m_window;
        VkSurfaceKHR m_surface{nullptr};

        instance m_instance;
        single_queue_engine m_engine;
        allocator m_allocator;

        resource_manager m_resourceManager;

        command_buffer_pool m_pools[SwapchainImages];
        swapchain<SwapchainImages> m_swapchain;
        handle<texture> m_swapchainTextures[SwapchainImages]{};

        u32 m_renderWidth;
        u32 m_renderHeight;

        VkSemaphore m_presentSemaphore{nullptr};
        VkSemaphore m_timelineSemaphore{nullptr};
        VkFence m_presentFences[SwapchainImages]{nullptr};

        u64 m_frameSemaphoreValues[SwapchainImages] = {0};
        u64 m_currentSemaphoreValue{0};

        imgui m_imgui;

        config m_config{};

        bool m_showImgui{true};
    };

    template <typename TApp>
    concept app_requiring_instance_extensions = requires(TApp app) {
        {
            app.get_required_instance_extensions()
        } -> std::convertible_to<std::span<const char* const>>;
    };

    template <typename TApp>
    concept app_requiring_physical_device_features = requires(TApp app) {
        {
            app.get_required_physical_device_features()
        } -> std::convertible_to<VkPhysicalDeviceFeatures>;
    };

    template <typename TApp>
    concept app_requiring_device_extensions = requires(TApp app) {
        {
            app.get_required_device_extensions()
        } -> std::convertible_to<std::span<const char* const>>;
    };

    template <typename TApp>
    concept app_requiring_device_features = requires(TApp app) {
        {
            app.get_device_features_list()
        } -> std::convertible_to<void*>;
    };

    template <typename TApp>
    class sandbox_app : sandbox_base, TApp
    {
    public:
        bool init()
        {
            std::span<const char* const> instanceExtensions;
            std::span<const char* const> instanceLayers;
            std::span<const char* const> deviceExtensions;
            void* deviceFeaturesList{nullptr};
            VkPhysicalDeviceFeatures physicalDeviceFeatures{};
            const VkPhysicalDeviceFeatures* pPhysicalDeviceFeatures{nullptr};

            if constexpr (app_requiring_instance_extensions<TApp>)
            {
                instanceExtensions = TApp::get_required_instance_extensions();
            }

            if constexpr (app_requiring_device_extensions<TApp>)
            {
                deviceExtensions = TApp::get_required_device_extensions();
            }

            if constexpr (app_requiring_device_features<TApp>)
            {
                deviceFeaturesList = TApp::get_device_features_list();
            }

            if constexpr (app_requiring_physical_device_features<TApp>)
            {
                physicalDeviceFeatures = TApp::get_required_physical_device_features();
                pPhysicalDeviceFeatures = &physicalDeviceFeatures;
            }

            const sandbox_init_context context{
                .engine = &m_engine,
                .allocator = &m_allocator,
                .swapchainFormat = SwapchainFormat,
            };

            return sandbox_base::init(instanceExtensions,
                                      instanceLayers,
                                      deviceExtensions,
                                      deviceFeaturesList,
                                      pPhysicalDeviceFeatures) &&
                   TApp::init(context);
        }

        void run()
        {
            // We start counting the frame indices from 1, because we use this value as timeline semaphore, which is
            // already signaled with 0
            for (u64 frameIndex{1};; ++frameIndex)
            {
                if (!poll_events())
                {
                    return;
                }

                const auto showImgui = m_showImgui;

                if (showImgui)
                {
                    m_imgui.begin_frame();

                    const sandbox_update_imgui_context context{
                        .engine = &m_engine,
                        .allocator = &m_allocator,
                    };

                    TApp::update_imgui(context);
                }

                u32 imageIndex;
                u32 poolIndex;
                begin_frame(frameIndex, &imageIndex, &poolIndex);

                constexpr u32 numCommandBuffers{2};
                VkCommandBuffer commandBuffers[numCommandBuffers];
                begin_command_buffers(poolIndex, commandBuffers, numCommandBuffers);

                const auto glueCommandBuffer = commandBuffers[0];
                const auto mainCommandBuffer = commandBuffers[1];

                stateful_command_buffer statefulCommandBuffer{mainCommandBuffer};

                const handle<texture> swapchainTexture = m_swapchainTextures[imageIndex];

                const sandbox_render_context context{
                    .engine = &m_engine,
                    .allocator = &m_allocator,
                    .resourceManager = &m_resourceManager,
                    .commandBuffer = &statefulCommandBuffer,
                    .swapchainTexture = swapchainTexture,
                    .width = m_renderWidth,
                    .height = m_renderHeight,
                    .frameIndex = frameIndex,
                };

                static_cast<TApp*>(this)->update(context);

                if (showImgui)
                {
                    m_imgui.end_frame(mainCommandBuffer,
                                      m_swapchain.get_image_view(imageIndex),
                                      m_renderWidth,
                                      m_renderHeight);
                }

                statefulCommandBuffer.add_pipeline_barrier(*context.resourceManager,
                                                           swapchainTexture,
                                                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

                m_resourceManager.commit(statefulCommandBuffer, glueCommandBuffer);

                end_command_buffers(commandBuffers, numCommandBuffers);
                submit_and_present(commandBuffers, numCommandBuffers, imageIndex, poolIndex, frameIndex);
            }
        }

        void shutdown()
        {
            wait_idle();

            const sandbox_shutdown_context context{.engine = &m_engine, .allocator = &m_allocator};
            TApp::shutdown(context);
            sandbox_base::shutdown();
        }
    };
}