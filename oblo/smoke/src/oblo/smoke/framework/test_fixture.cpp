#include <oblo/smoke/framework/test_fixture.hpp>

#include <oblo/app/imgui_app.hpp>
#include <oblo/app/imgui_texture.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importers/importers_module.hpp>
#include <oblo/asset/utility/registration.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/platform/shared_library.hpp>
#include <oblo/core/platform/shell.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/input/utility/fps_camera_controller.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/renderdoc/renderdoc_module.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/resource/utility/registration.hpp>
#include <oblo/runtime/runtime.hpp>
#include <oblo/runtime/runtime_module.hpp>
#include <oblo/runtime/runtime_registry.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>
#include <oblo/smoke/framework/test.hpp>
#include <oblo/smoke/framework/test_context.hpp>
#include <oblo/smoke/framework/test_context_impl.hpp>
#include <oblo/smoke/framework/test_task.hpp>
#include <oblo/thread/job_manager.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

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
            input_queue inputQueue{};
            test_context_impl* testCtx{};
            string testName;

            imgui_app imguiApp;

            bool init(string_view name)
            {
                testName = name;

                auto& mm = module_manager::get();

                mm.load<options_module>();
                mm.load<runtime_module>();
                mm.load<reflection::reflection_module>();
                mm.load<scene_module>();
                mm.load<importers::importers_module>();
                mm.load<vk::vulkan_engine_module>();

                mm.finalize();

                return startup();
            }

            bool startup()
            {
                filesystem::remove_all("./test/smoke/").assert_value();

                if (!assetRegistry.initialize("./test/smoke/assets", "./test/smoke/artifacts", "./test/smoke/sources"))
                {
                    return false;
                }

                auto& mm = module_manager::get();

                auto* const runtimeModule = mm.find<runtime_module>();
                auto* const reflectionModule = mm.find<reflection::reflection_module>();

                runtimeRegistry = runtimeModule->create_runtime_registry();

                auto& propertyRegistry = runtimeRegistry.get_property_registry();
                auto& resourceRegistry = runtimeRegistry.get_resource_registry();

                register_file_importers(assetRegistry, mm.find_services<file_importers_provider>());
                register_resource_types(resourceRegistry, mm.find_services<resource_types_provider>());

                assetRegistry.discover_assets({});

                resourceRegistry.register_provider(assetRegistry.initialize_resource_provider());

                if (!runtime.init({
                        .reflectionRegistry = &reflectionModule->get_registry(),
                        .propertyRegistry = &propertyRegistry,
                        .resourceRegistry = &resourceRegistry,
                        .renderer = mm.find<vk::vulkan_engine_module>()->get_renderer(),
                        .worldBuilders = mm.find_services<ecs::world_builder>(),
                    }))
                {
                    return false;
                }

                if (!imguiApp.init({.title = testName},
                        {
                            .configFile = nullptr,
                            .useMultiViewport = false,
                            .useDocking = false,
                            .useKeyboardNavigation = false,
                        }) ||
                    !imguiApp.init_font_atlas(resourceRegistry))
                {
                    return false;
                }

                return true;
            }

            void shutdown()
            {
                imguiApp.shutdown();

                runtime.shutdown();
            }

            bool run_frame()
            {
                const auto [w, h] = imguiApp.get_main_window().get_size();

                auto& viewport = runtime.get_entity_registry().get<viewport_component>(cameraEntity);

                viewport.width = w;
                viewport.height = h;

                handle_renderdoc_captures();

                assetRegistry.update();
                runtimeRegistry.get_resource_registry().update();

                inputQueue.clear();

                if (!imguiApp.process_events())
                {
                    return false;
                }

                while (!imguiApp.acquire_images())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds{10});
                }

                runtime.update({.dt = FixedTime});

                update_imgui();

                imguiApp.present();

                return true;
            }

            void update_imgui()
            {
                imguiApp.begin_ui();

                if (cameraEntity)
                {
                    auto& viewport = runtime.get_entity_registry().get<viewport_component>(cameraEntity);
                    OBLO_ASSERT(viewport.graph);

                    if (viewport.graph)
                    {
                        const auto viewportSize = ImVec2{f32(viewport.width), f32(viewport.height)};

                        ImGui::SetNextWindowPos({});
                        ImGui::SetNextWindowSize(viewportSize);

                        if (bool open{true}; ImGui::Begin("fullscreen",
                                &open,
                                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground))
                        {
                            const auto image = imgui::add_image(viewport.graph, "LitOutput");

                            ImGui::Image(image, viewportSize);
                            ImGui::End();
                        }
                    }
                }

                imguiApp.end_ui();
            }

            void set_input_processing(bool enable)
            {
                imguiApp.set_input_queue(enable ? &inputQueue : nullptr);
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
        job_manager jobManager;
        oblo::module_manager moduleManager;
        test_app app;

        impl()
        {
            jobManager.init();
        }

        ~impl()
        {
            app.shutdown();
            module_manager::get().shutdown();
            jobManager.shutdown();
        }
    };

    test_fixture::test_fixture() = default;
    test_fixture::~test_fixture() = default;

    bool test_fixture::init(const test_fixture_config& cfg)
    {
        m_impl = std::make_unique<impl>();
        auto& app = m_impl->app;

        if (!app.init(cfg.name))
        {
            return false;
        }

        auto& entities = app.runtime.get_entity_registry();

        app.cameraEntity = ecs_utility::create_named_physical_entity<camera_component, viewport_component>(entities,
            "Camera",
            {},
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
            controller.process(app.inputQueue.get_events(), dt);

            position.value = controller.get_position();
            rotation.value = controller.get_orientation();

            entities.notify(app.cameraEntity);
        }
    }
}