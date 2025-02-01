#pragma once

#include <concepts>

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/sandbox/context.hpp>
#include <oblo/sandbox/imgui.hpp>
#include <oblo/sandbox/sandbox_app_config.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
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

    protected:
        using update_fn = void (*)(void*, const sandbox_render_context&);
        using update_imgui_fn = void (*)(void*, const sandbox_update_imgui_context&);

        bool run_frame_impl(void* instance, update_fn update, update_imgui_fn updateImgui);

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

        void create_debug_callbacks();
        void destroy_debug_callbacks();

        bool handle_window_events(const SDL_Event& event);

    protected:
        static constexpr u32 SwapchainImages{3u};
        static constexpr VkFormat SwapchainFormat{VK_FORMAT_B8G8R8A8_UNORM};

        SDL_Window* m_window;
        VkSurfaceKHR m_surface{nullptr};

        input_queue m_inputQueue;

        instance m_instance;
        single_queue_engine m_engine;
        gpu_allocator m_allocator;

        resource_manager m_resourceManager;

        vulkan_context m_context;

        swapchain<SwapchainImages> m_swapchain;
        h32<texture> m_swapchainTextures[SwapchainImages]{};

        u32 m_renderWidth{};
        u32 m_renderHeight{};

        u32 m_semaphoreIndex{};

        VkSemaphore m_acquiredImages[SwapchainImages]{};
        VkSemaphore m_frameCompleted[SwapchainImages]{};

        VkDebugUtilsMessengerEXT m_vkMessenger{};

        imgui m_imgui;

        sandbox_app_config m_config{};

        bool m_minimized{false};
        bool m_showImgui{true};
        bool m_processInput{true};
    };

    template <typename TApp, typename... TArgs>
    concept app_has_init = requires(TApp app, TArgs... args) {
        { app.init(args...) } -> std::convertible_to<bool>;
    };

    template <typename TApp>
    concept app_requiring_instance_extensions = requires(TApp app) {
        { app.get_required_instance_extensions() } -> std::convertible_to<std::span<const char* const>>;
    };

    template <typename TApp>
    concept app_requiring_physical_device_features = requires(TApp app) {
        { app.get_required_physical_device_features() } -> std::convertible_to<VkPhysicalDeviceFeatures2>;
    };

    template <typename TApp>
    concept app_requiring_device_extensions = requires(TApp app) {
        { app.get_required_device_extensions() } -> std::convertible_to<std::span<const char* const>>;
    };

    template <typename TApp>
    concept app_requiring_device_features = requires(TApp app) {
        { app.get_required_device_features() } -> std::convertible_to<void*>;
    };

    template <typename TApp>
    class sandbox_app : sandbox_base, public TApp
    {
    public:
        using sandbox_base::set_config;

    public:
        template <typename... TArgs>
        bool init(TArgs&&... args)
        {
            //if constexpr (app_has_init<TApp, TArgs...>)
            {
                if (!TApp::init(std::forward<TArgs>(args)...))
                {
                    return false;
                }
            }

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
                deviceFeaturesList = TApp::get_required_device_features();
            }

            if constexpr (app_requiring_physical_device_features<TApp>)
            {
                physicalDeviceFeatures = TApp::get_required_physical_device_features();
                pPhysicalDeviceFeatures = &physicalDeviceFeatures;
            }

            const sandbox_startup_context context{
                .vkContext = &m_context,
                .inputQueue = &m_inputQueue,
                .swapchainFormat = SwapchainFormat,
            };

            m_isFirstUpdate = true;

            return sandbox_base::init(instanceExtensions,
                       instanceLayers,
                       deviceExtensions,
                       deviceFeaturesList,
                       pPhysicalDeviceFeatures) &&
                TApp::startup(context);
        }

        void run()
        {
            while (run_frame())
            {
            }
        }

        bool run_frame()
        {
            constexpr update_fn updateCbs[] = {
                [](void* instance, const sandbox_render_context& context)
                {
                    auto* const self = static_cast<TApp*>(instance);
                    self->update(context);
                },
                [](void* instance, const sandbox_render_context& context)
                {
                    auto* const self = static_cast<TApp*>(instance);

                    if constexpr (requires(TApp& app, const sandbox_render_context& context) {
                                      app.first_update(context);
                                  })
                    {
                        self->first_update(context);
                    }
                    else
                    {
                        self->update(context);
                    }
                },
            };

            constexpr update_imgui_fn updateImgui = [](void* instance, const sandbox_update_imgui_context& context)
            {
                auto* const self = static_cast<TApp*>(instance);
                self->update_imgui(context);
            };

            const auto updateCb = updateCbs[u32{m_isFirstUpdate}];
            m_isFirstUpdate = false;

            return run_frame_impl(static_cast<TApp*>(this), updateCb, updateImgui);
        }

        void shutdown()
        {
            wait_idle();

            const sandbox_shutdown_context context{
                .vkContext = &m_context,
            };

            TApp::shutdown(context);
            sandbox_base::shutdown();
        }

        void set_input_processing(bool enable)
        {
            m_processInput = enable;
        }

    private:
        bool m_isFirstUpdate{true};
    };
}