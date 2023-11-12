#pragma once

#include <concepts>

#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <oblo/sandbox/context.hpp>
#include <oblo/sandbox/imgui.hpp>
#include <oblo/sandbox/sandbox_app_config.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/swapchain.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

struct SDL_Window;
union SDL_Event;

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

        void set_config(const sandbox_app_config& config);

        bool init(std::span<const char* const> instanceExtensions,
            std::span<const char* const> instanceLayers,
            std::span<const char* const> deviceExtensions,
            void* deviceFeaturesList,
            const VkPhysicalDeviceFeatures2* physicalDeviceFeatures);

        void shutdown();

        void wait_idle();

        bool poll_events();

        void begin_frame(u32* outImageIndex);

        void submit_and_present(u32 imageIndex);

    private:
        bool create_window();

        bool create_engine(std::span<const char* const> instanceExtensions,
            std::span<const char* const> instanceLayers,
            std::span<const char* const> deviceExtensions,
            void* deviceFeaturesList,
            const VkPhysicalDeviceFeatures2* physicalDeviceFeatures);

        bool create_swapchain();
        bool create_synchronization_objects();
        bool init_imgui();

        void destroy_swapchain();

        bool handle_window_events(const SDL_Event& event);

    protected:
        static constexpr u32 SwapchainImages{2u};
        static constexpr VkFormat SwapchainFormat{VK_FORMAT_B8G8R8A8_UNORM};

        frame_allocator m_frameAllocator;

        SDL_Window* m_window;
        VkSurfaceKHR m_surface{nullptr};

        instance m_instance;
        single_queue_engine m_engine;
        allocator m_allocator;

        resource_manager m_resourceManager;

        vulkan_context m_context;

        swapchain<SwapchainImages> m_swapchain;
        h32<texture> m_swapchainTextures[SwapchainImages]{};

        u32 m_renderWidth;
        u32 m_renderHeight;

        VkSemaphore m_presentSemaphore{nullptr};

        u64 m_currentSemaphoreValue{0};

        imgui m_imgui;

        sandbox_app_config m_config{};

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
        } -> std::convertible_to<VkPhysicalDeviceFeatures2>;
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
    class sandbox_app : sandbox_base, public TApp
    {
    public:
        using sandbox_base::set_config;

    public:
        bool init()
        {
            std::span<const char* const> instanceExtensions;
            std::span<const char* const> instanceLayers;
            std::span<const char* const> deviceExtensions;
            void* deviceFeaturesList{nullptr};
            VkPhysicalDeviceFeatures2 physicalDeviceFeatures{};
            const VkPhysicalDeviceFeatures2* pPhysicalDeviceFeatures{nullptr};

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
                .vkContext = &m_context,
                .frameAllocator = &m_frameAllocator,
                .swapchainFormat = SwapchainFormat,
            };

            if constexpr (requires(TApp& app, const sandbox_render_context& context) { app.first_update(context); })
            {
                m_updateCb = &TApp::first_update;
            }
            else
            {
                m_updateCb = &TApp::update;
            }

            return sandbox_base::init(instanceExtensions,
                       instanceLayers,
                       deviceExtensions,
                       deviceFeaturesList,
                       pPhysicalDeviceFeatures) &&
                TApp::init(context);
        }

        void run()
        {
            while (run_frame())
            {
            }
        }

        bool run_frame()
        {
            if (!poll_events())
            {
                return false;
            }

            u32 imageIndex;
            begin_frame(&imageIndex);

            const h32<texture> swapchainTexture = m_swapchainTextures[imageIndex];

            const sandbox_render_context context{
                .vkContext = &m_context,
                .frameAllocator = &m_frameAllocator,
                .swapchainTexture = swapchainTexture,
                .width = m_renderWidth,
                .height = m_renderHeight,
            };

            (static_cast<TApp*>(this)->*m_updateCb)(context);
            m_updateCb = &TApp::update;

            auto& cb = m_context.get_active_command_buffer();

            if (m_showImgui)
            {
                m_imgui.begin_frame();

                const sandbox_update_imgui_context context{
                    .vkContext = &m_context,
                };

                TApp::update_imgui(context);

                m_imgui.end_frame(cb.get(), m_swapchain.get_image_view(imageIndex), m_renderWidth, m_renderHeight);
            }

            cb.add_pipeline_barrier(m_resourceManager, swapchainTexture, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            submit_and_present(imageIndex);
            return true;
        }

        void shutdown()
        {
            wait_idle();

            const sandbox_shutdown_context context{
                .vkContext = &m_context,
                .frameAllocator = &m_frameAllocator,
            };

            TApp::shutdown(context);
            sandbox_base::shutdown();
        }

    private:
        void (TApp::*m_updateCb)(const sandbox_render_context&){};
    };
}