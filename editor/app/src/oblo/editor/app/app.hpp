#pragma once

#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/editor/data/time_stats.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/runtime/runtime.hpp>
#include <oblo/runtime/runtime_registry.hpp>
#include <oblo/thread/job_manager.hpp>

#include <span>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    struct sandbox_startup_context;
    struct sandbox_shutdown_context;
    struct sandbox_render_context;
    struct sandbox_update_imgui_context;
}

namespace oblo
{
    class options_manager;
    struct options_layer;
}

namespace oblo::editor
{
    class log_queue;

    class options_layer_helper
    {
    public:
        void init();
        void load();
        void save();
        void update();

    private:
        options_manager* m_options{};
        h32<options_layer> m_layer{};
        u32 m_changeId{};
    };

    class app
    {
    public:
        std::span<const char* const> get_required_instance_extensions() const;

        VkPhysicalDeviceFeatures2 get_required_physical_device_features() const;

        void* get_required_device_features() const;

        std::span<const char* const> get_required_device_extensions() const;

        bool init();

        bool startup(const vk::sandbox_startup_context& context);

        void shutdown(const vk::sandbox_shutdown_context& context);

        void update(const vk::sandbox_render_context& context);

        void update_imgui(const vk::sandbox_update_imgui_context& context);

    private:
        const log_queue* m_logQueue{};
        job_manager m_jobManager;
        window_manager m_windowManager;
        runtime_registry m_runtimeRegistry;
        runtime m_runtime;
        asset_registry m_assetRegistry;
        time_stats m_timeStats{};
        time m_lastFrameTime{};

        options_layer_helper m_editorOptions;
    };
}