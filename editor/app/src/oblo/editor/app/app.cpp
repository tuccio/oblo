#include "app.hpp"

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importers/importers_module.hpp>
#include <oblo/asset/registration.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/app/commands.hpp>
#include <oblo/editor/editor_module.hpp>
#include <oblo/editor/services/component_factory.hpp>
#include <oblo/editor/services/registered_commands.hpp>
#include <oblo/editor/ui/style.hpp>
#include <oblo/editor/windows/asset_browser.hpp>
#include <oblo/editor/windows/inspector.hpp>
#include <oblo/editor/windows/scene_editing_window.hpp>
#include <oblo/editor/windows/scene_hierarchy.hpp>
#include <oblo/editor/windows/style_window.hpp>
#include <oblo/editor/windows/viewport.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/resource/registration.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/runtime/runtime_module.hpp>
#include <oblo/sandbox/context.hpp>
#include <oblo/thread/job_manager.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/required_features.hpp>

namespace oblo::editor
{
    std::span<const char* const> app::get_required_instance_extensions() const
    {
        return runtime::get_required_vulkan_features().instanceExtensions;
    }

    VkPhysicalDeviceFeatures2 app::get_required_physical_device_features() const
    {
        return runtime::get_required_vulkan_features().physicalDeviceFeatures;
    }

    void* app::get_required_device_features() const
    {
        return runtime::get_required_vulkan_features().deviceFeaturesChain;
    }

    std::span<const char* const> app::get_required_device_extensions() const
    {
        return runtime::get_required_vulkan_features().deviceExtensions;
    }

    bool app::init()
    {
        if (!platform::init())
        {
            return false;
        }

        m_jobManager.init();
        return true;
    }

    bool app::startup(const vk::sandbox_startup_context& ctx)
    {
        init_ui_style();

        auto& mm = module_manager::get();
        auto* const runtime = mm.load<oblo::runtime_module>();
        auto* const reflection = mm.load<oblo::reflection::reflection_module>();
        mm.load<importers::importers_module>();
        mm.load<editor_module>();

        m_runtimeRegistry = runtime->create_runtime_registry();

        // TODO: Load a project instead
        if (!m_assetRegistry.initialize("./project/assets", "./project/artifacts", "./project/sources"))
        {
            return false;
        }

        auto& propertyRegistry = m_runtimeRegistry.get_property_registry();
        auto& resourceRegistry = m_runtimeRegistry.get_resource_registry();

        register_asset_types(m_assetRegistry, mm.find_services<resource_types_provider>());
        register_file_importers(m_assetRegistry, mm.find_services<file_importers_provider>());
        register_resource_types(resourceRegistry, mm.find_services<resource_types_provider>());

        m_assetRegistry.discover_assets();

        resourceRegistry.register_provider(&asset_registry::find_artifact_resource, &m_assetRegistry);

        if (!m_runtime.init({
                .reflectionRegistry = &reflection->get_registry(),
                .propertyRegistry = &propertyRegistry,
                .resourceRegistry = &resourceRegistry,
                .vulkanContext = ctx.vkContext,
                .worldBuilders = mm.find_services<ecs::world_builder>(),
            }))
        {
            return false;
        }

        auto& renderer = m_runtime.get_renderer();

        m_windowManager.init();

        {
            auto& globalRegistry = m_windowManager.get_global_service_registry();

            globalRegistry.add<vk::vulkan_context>().externally_owned(ctx.vkContext);
            globalRegistry.add<vk::renderer>().externally_owned(&renderer);
            globalRegistry.add<resource_registry>().externally_owned(&resourceRegistry);
            globalRegistry.add<asset_registry>().externally_owned(&m_assetRegistry);
            globalRegistry.add<property_registry>().externally_owned(&propertyRegistry);
            globalRegistry.add<const reflection::reflection_registry>().externally_owned(&reflection->get_registry());
            globalRegistry.add<const input_queue>().externally_owned(ctx.inputQueue);
            globalRegistry.add<component_factory>().unique();
            globalRegistry.add<const time_stats>().externally_owned(&m_timeStats);

            auto* const registeredCommands = globalRegistry.add<registered_commands>().unique();
            fill_commands(*registeredCommands);

            service_registry sceneRegistry{};
            sceneRegistry.add<ecs::entity_registry>().externally_owned(&m_runtime.get_entity_registry());

            const auto sceneEditingWindow =
                m_windowManager.create_window<scene_editing_window>(std::move(sceneRegistry));

            m_windowManager.create_child_window<asset_browser>(sceneEditingWindow);
            m_windowManager.create_child_window<inspector>(sceneEditingWindow);
            m_windowManager.create_child_window<scene_hierarchy>(sceneEditingWindow);
            m_windowManager.create_child_window<viewport>(sceneEditingWindow);
        }

        m_lastFrameTime = clock::now();

        return true;
    }

    void app::shutdown(const vk::sandbox_shutdown_context&)
    {
        m_windowManager.shutdown();
        m_runtime.shutdown();
        platform::shutdown();

        m_jobManager.shutdown();
    }

    void app::update(const vk::sandbox_render_context&)
    {
        const auto now = clock::now();
        const auto dt = now - m_lastFrameTime;

        m_timeStats.dt = dt;

        m_runtime.update({.dt = dt});
        m_lastFrameTime = now;
    }

    void app::update_imgui(const vk::sandbox_update_imgui_context&)
    {
        m_windowManager.update();
    }
}