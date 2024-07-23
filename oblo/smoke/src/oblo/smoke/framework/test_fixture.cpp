#include <oblo/smoke/framework/test_fixture.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/registration.hpp>
#include <oblo/core/log.hpp>
#include <oblo/core/platform/shared_library.hpp>
#include <oblo/core/platform/shell.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/input/utility/fps_camera_controller.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/renderdoc/renderdoc_module.hpp>
#include <oblo/resource/registration.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/runtime/runtime.hpp>
#include <oblo/runtime/runtime_module.hpp>
#include <oblo/runtime/runtime_registry.hpp>
#include <oblo/sandbox/sandbox_app.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>
#include <oblo/smoke/framework/test.hpp>
#include <oblo/smoke/framework/test_context.hpp>
#include <oblo/smoke/framework/test_context_impl.hpp>
#include <oblo/smoke/framework/test_task.hpp>
#include <oblo/vulkan/required_features.hpp>

#include <imgui.h>

#include <renderdoc_app.h>

namespace oblo::smoke
{
    namespace
    {
        struct test_app
        {
            static constexpr time FixedTime = time::from_seconds(1.f / 60);

            asset_registry assetRegistry;
            runtime_registry runtimeRegistry;
            runtime runtime;
            ecs::entity cameraEntity{};
            const input_queue* inputQueue{};
            test_context_impl* testCtx{};

            std::span<const char* const> get_required_instance_extensions() const
            {
                return runtime::get_required_vulkan_features().instanceExtensions;
            }

            VkPhysicalDeviceFeatures2 get_required_physical_device_features() const
            {
                return runtime::get_required_vulkan_features().physicalDeviceFeatures;
            }

            void* get_required_device_features() const
            {
                return runtime::get_required_vulkan_features().deviceFeaturesChain;
            }

            std::span<const char* const> get_required_device_extensions() const
            {
                return runtime::get_required_vulkan_features().deviceExtensions;
            }

            bool startup(const vk::sandbox_startup_context& ctx)
            {
                {
                    std::error_code ec;
                    std::filesystem::remove_all("./test/smoke/", ec);
                }

                if (!assetRegistry.initialize("./test/smoke/assets", "./test/smoke/artifacts", "./test/smoke/sources"))
                {
                    return false;
                }

                auto& mm = module_manager::get();

                auto* const runtimeModule = mm.load<runtime_module>();
                auto* const reflectionModule = mm.load<reflection::reflection_module>();

                runtimeRegistry = runtimeModule->create_runtime_registry();

                auto& propertyRegistry = runtimeRegistry.get_property_registry();
                auto& resourceRegistry = runtimeRegistry.get_resource_registry();

                register_asset_types(assetRegistry, mm.find_services<resource_types_provider>());
                register_file_importers(assetRegistry, mm.find_services<file_importers_provider>());
                register_resource_types(resourceRegistry, mm.find_services<resource_types_provider>());

                assetRegistry.discover_assets();

                resourceRegistry.register_provider(&asset_registry::find_artifact_resource, &assetRegistry);

                if (!runtime.init({
                        .reflectionRegistry = &reflectionModule->get_registry(),
                        .propertyRegistry = &propertyRegistry,
                        .resourceRegistry = &resourceRegistry,
                        .vulkanContext = ctx.vkContext,
                        .worldBuilders = mm.find_services<ecs::world_builder>(),
                    }))
                {
                    return false;
                }

                inputQueue = ctx.inputQueue;

                return true;
            }

            void shutdown(const vk::sandbox_shutdown_context&)
            {
                runtime.shutdown();
            }

            void update(const vk::sandbox_render_context& ctx)
            {
                handle_renderdoc_captures();

                auto& viewport = runtime.get_entity_registry().get<viewport_component>(cameraEntity);
                viewport.width = ctx.width;
                viewport.height = ctx.height;

                runtime.update({.dt = FixedTime});

                auto& resourceManager = ctx.vkContext->get_resource_manager();
                auto& commandBuffer = ctx.vkContext->get_active_command_buffer();

                commandBuffer.add_pipeline_barrier(resourceManager,
                    ctx.swapchainTexture,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                constexpr VkClearColorValue black{};

                const auto& texture = resourceManager.get(ctx.swapchainTexture);

                const VkImageSubresourceRange range{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                };

                vkCmdClearColorImage(commandBuffer.get(),
                    texture.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &black,
                    1,
                    &range);
            }

            void update_imgui(const vk::sandbox_update_imgui_context&)
            {
                if (cameraEntity)
                {
                    auto& viewport = runtime.get_entity_registry().get<viewport_component>(cameraEntity);

                    if (viewport.imageId)
                    {
                        const auto viewportSize = ImVec2{f32(viewport.width), f32(viewport.height)};

                        ImGui::SetNextWindowPos({});
                        ImGui::SetNextWindowSize(viewportSize);

                        if (bool open{true}; ImGui::Begin("fullscreen",
                                &open,
                                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground))
                        {
                            ImGui::Image(viewport.imageId, viewportSize);
                            ImGui::End();
                        }
                    }
                }
            }

            bool isRenderDocFirstUsage{true};
            bool isRenderDocCapturing{};

            renderdoc_module::api* renderdocApi{};

            void handle_renderdoc_captures()
            {
                if (isRenderDocCapturing)
                {
                    renderdocApi->EndFrameCapture(nullptr, nullptr);
                }

                if (testCtx && testCtx->renderdocCapture)
                {
                    if (isRenderDocFirstUsage)
                    {
                        auto* const renderdoc = module_manager::get().load<renderdoc_module>();

                        if (renderdoc)
                        {
                            renderdocApi = renderdoc->get_api();
                        }
                    }

                    if (renderdocApi)
                    {
                        renderdocApi->StartFrameCapture(nullptr, nullptr);
                        isRenderDocCapturing = true;
                    }

                    testCtx->renderdocCapture = false;
                }
            }
        };
    }

    struct test_fixture::impl
    {
        vk::sandbox_app<test_app> app;

        ~impl()
        {
            app.shutdown();
        }
    };

    test_fixture::test_fixture() = default;
    test_fixture::~test_fixture() = default;

    bool test_fixture::init(const test_fixture_config& cfg)
    {
        m_impl = std::make_unique<impl>();
        auto& app = m_impl->app;

        app.set_config({
            .appName = cfg.name,
            .appMainWindowTitle = cfg.name,
        });

        if (!app.init())
        {
            return false;
        }

        auto& entities = app.runtime.get_entity_registry();

        app.cameraEntity = ecs_utility::create_named_physical_entity<camera_component, viewport_component>(entities,
            "Camera",
            {},
            quaternion::identity(),
            vec3::splat(1));

        auto& camera = entities.get<camera_component>(app.cameraEntity);
        camera.near = 0.01f;
        camera.far = 1000.f;
        camera.fovy = 75_deg;

        return true;
    }

    bool test_fixture::run_test(test& test)
    {
        auto& app = m_impl->app;

        test_context_impl impl{
            .entities = &app.runtime.get_entity_registry(),
            .assetRegistry = &app.assetRegistry,
            .resourceRegistry = &app.runtimeRegistry.get_resource_registry(),
        };

        const test_context ctx{&impl};
        const auto task = test.run(ctx);

        app.set_input_processing(false);
        app.testCtx = &impl;

        bool shouldQuit{false};

        while (!task.is_done())
        {
            task.resume();

            if (!app.run_frame())
            {
                shouldQuit = true;
                break;
            }
        }

        // One extra frame in order to swap buffers and show the final frame
        if (!shouldQuit)
        {
            app.run_frame();
        }

        app.testCtx = nullptr;
        return true;
    }

    void test_fixture::run_interactive()
    {
        auto& app = m_impl->app;

        app.set_input_processing(true);

        fps_camera_controller controller;
        controller.set_common_wasd_bindings();

        auto& entities = app.runtime.get_entity_registry();

        {
            const auto& [position, rotation] = entities.get<position_component, rotation_component>(app.cameraEntity);
            controller.reset(position.value, rotation.value);
        }

        time lastFrame = clock::now();

        while (app.run_frame())
        {
            const auto dt = clock::now() - lastFrame;

            auto&& [position, rotation, viewport] =
                entities.get<position_component, rotation_component, viewport_component>(app.cameraEntity);

            controller.set_screen_size({f32(viewport.width), f32(viewport.height)});
            controller.process(app.inputQueue->get_events(), dt);

            position.value = controller.get_position();
            rotation.value = controller.get_orientation();
        }
    }
}