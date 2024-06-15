#pragma once

#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/editor/data/time_stats.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/runtime/runtime.hpp>
#include <oblo/runtime/runtime_registry.hpp>

#include <span>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    struct sandbox_init_context;
    struct sandbox_shutdown_context;
    struct sandbox_render_context;
    struct sandbox_update_imgui_context;
}

namespace oblo::editor
{
    class app
    {
    public:
        std::span<const char* const> get_required_instance_extensions() const;

        VkPhysicalDeviceFeatures2 get_required_physical_device_features() const;

        void* get_required_device_features() const;

        std::span<const char* const> get_required_device_extensions() const;

        bool init(const vk::sandbox_init_context& context);

        void shutdown(const vk::sandbox_shutdown_context& context);

        void update(const vk::sandbox_render_context& context);

        void update_imgui(const vk::sandbox_update_imgui_context& context);

    private:
        window_manager m_windowManager;
        runtime_registry m_runtimeRegistry;
        runtime m_runtime;
        asset_registry m_assetRegistry;
        time_stats m_timeStats{};
        time m_lastFrameTime{};
    };
}