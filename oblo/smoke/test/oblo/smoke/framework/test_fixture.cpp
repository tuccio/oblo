#include <oblo/smoke/framework/test_fixture.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/registration.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/resource/registration.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/runtime/runtime.hpp>
#include <oblo/runtime/runtime_module.hpp>
#include <oblo/sandbox/sandbox_app.hpp>
#include <oblo/smoke/framework/test.hpp>
#include <oblo/smoke/framework/test_context.hpp>
#include <oblo/smoke/framework/test_context_impl.hpp>
#include <oblo/smoke/framework/test_task.hpp>

namespace oblo::smoke
{
    namespace
    {
        // These are actually required by runtime, so they should probably be taken from there somehow
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

        struct test_app
        {
            asset_registry assetRegistry;
            runtime runtime;
            resource_registry* resourceRegistry{};

            std::span<const char* const> get_required_instance_extensions() const
            {
                return InstanceExtensions;
            }

            VkPhysicalDeviceFeatures2 get_required_physical_device_features() const
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

            void* get_required_device_features() const
            {
                return &IndexingFeatures;
            }

            std::span<const char* const> get_required_device_extensions() const
            {
                return DeviceExtensions;
            }

            bool init(const vk::sandbox_init_context& ctx)
            {
                // TODO: Wipe test folder
                std::error_code ec;
                std::filesystem::remove_all("./smoke_tests/project/", ec);

                if (!assetRegistry.initialize("./smoke_tests/project/assets",
                        "./smoke_tests/project/artifacts",
                        "./smoke_tests/project/sources"))
                {
                    return false;
                }

                auto& mm = module_manager::get();

                auto* const runtimeModule = mm.load<runtime_module>();
                auto* const reflectionModule = mm.load<reflection::reflection_module>();

                auto& propertyRegistry = runtimeModule->get_property_registry();
                resourceRegistry = &runtimeModule->get_resource_registry();

                register_asset_types(assetRegistry, mm.find_services<resource_types_provider>());
                register_file_importers(assetRegistry, mm.find_services<file_importers_provider>());
                register_resource_types(*resourceRegistry, mm.find_services<resource_types_provider>());

                assetRegistry.discover_assets();

                resourceRegistry->register_provider(&asset_registry::find_artifact_resource, &assetRegistry);

                if (!runtime.init({
                        .reflectionRegistry = &reflectionModule->get_registry(),
                        .propertyRegistry = &propertyRegistry,
                        .resourceRegistry = resourceRegistry,
                        .vulkanContext = ctx.vkContext,
                    }))
                {
                    return false;
                }

                return true;
            }

            void shutdown(const vk::sandbox_shutdown_context&)
            {
                runtime.shutdown();
            }

            void update(const vk::sandbox_render_context&)
            {
                runtime.update({});
            }

            void update_imgui(const vk::sandbox_update_imgui_context&) {}
        };
    }

    bool test_fixture::run_test(test& test)
    {
        vk::sandbox_app<test_app> app;

        if (!app.init())
        {
            return false;
        }

        const auto cleanup = finally([&app] { app.shutdown(); });

        const test_context_impl impl{
            .entities = &app.runtime.get_entity_registry(),
            .assetRegistry = &app.assetRegistry,
            .resourceRegistry = app.resourceRegistry,
        };

        const test_context ctx{&impl};
        const auto task = test.run(ctx);

        while (!task.is_done())
        {
            task.resume();
            app.run_frame();
        }

        return true;
    }
}