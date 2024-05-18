#include "app.hpp"

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importers/importers_module.hpp>
#include <oblo/asset/registration.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/services/component_factory.hpp>
#include <oblo/editor/ui/style.hpp>
#include <oblo/editor/windows/asset_browser.hpp>
#include <oblo/editor/windows/inspector.hpp>
#include <oblo/editor/windows/main_window.hpp>
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
#include <oblo/vulkan/draw/resource_cache.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::editor
{
    namespace
    {
        VkPhysicalDeviceDescriptorIndexingFeatures IndexingFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
            .descriptorBindingSampledImageUpdateAfterBind = true,
            .descriptorBindingPartiallyBound = true,
            .descriptorBindingVariableDescriptorCount = true,
            .runtimeDescriptorArray = true,
        };

        VkPhysicalDeviceShaderDrawParametersFeatures ShaderDrawParameters{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
            .shaderDrawParameters = true,
        };

        constexpr const char* InstanceExtensions[] = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        };

        constexpr const char* DeviceExtensions[] = {
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
            VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, // This is only needed for debug printf
        };
    }

    std::span<const char* const> app::get_required_instance_extensions() const
    {
        return InstanceExtensions;
    }

    VkPhysicalDeviceFeatures2 app::get_required_physical_device_features() const
    {
        return {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &ShaderDrawParameters,
            .features =
                {
                    .multiDrawIndirect = true,
                    .shaderInt64 = true,
                },
        };
    }

    void* app::get_required_device_features() const
    {
        return &IndexingFeatures;
    }

    std::span<const char* const> app::get_required_device_extensions() const
    {
        return DeviceExtensions;
    }

    bool app::init(const vk::sandbox_init_context& ctx)
    {
        if (!platform::init())
        {
            return false;
        }

        init_ui_style();

        auto& mm = module_manager::get();
        auto* const runtime = mm.load<oblo::runtime_module>();
        auto* const reflection = mm.load<oblo::reflection::reflection_module>();
        mm.load<importers::importers_module>();

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
            globalRegistry.add<const input_queue>().externally_owned(ctx.inputQueue);
            globalRegistry.add<component_factory>().unique();

            service_registry sceneRegistry{};
            sceneRegistry.add<ecs::entity_registry>().externally_owned(&m_runtime.get_entity_registry());

            const auto sceneEditingWindow =
                m_windowManager.create_window<scene_editing_window>(std::move(sceneRegistry));

            m_windowManager.create_child_window<asset_browser>(sceneEditingWindow);
            m_windowManager.create_child_window<inspector>(sceneEditingWindow);
            m_windowManager.create_child_window<scene_hierarchy>(sceneEditingWindow);
            m_windowManager.create_child_window<viewport>(sceneEditingWindow);
        }

        return true;
    }

    void app::shutdown(const vk::sandbox_shutdown_context&)
    {
        m_runtime.shutdown();
        m_windowManager.shutdown();
        platform::shutdown();
    }

    void app::update(const vk::sandbox_render_context&)
    {
        m_runtime.update({});
    }

    void app::update_imgui(const vk::sandbox_update_imgui_context&)
    {
        m_windowManager.update();
    }
}