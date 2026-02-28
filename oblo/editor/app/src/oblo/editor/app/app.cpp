#include "app.hpp"

#include <oblo/app/imgui_app.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/descriptors/asset_repository_descriptor.hpp>
#include <oblo/asset/importers/importers_module.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/asset/utility/registration.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/editor_module.hpp>
#include <oblo/editor/providers/module_repository_provider.hpp>
#include <oblo/editor/providers/service_provider.hpp>
#include <oblo/editor/services/asset_editor_manager.hpp>
#include <oblo/editor/services/component_factory.hpp>
#include <oblo/editor/services/editor_directories.hpp>
#include <oblo/editor/services/incremental_id_pool.hpp>
#include <oblo/editor/services/log_queue.hpp>
#include <oblo/editor/services/registered_commands.hpp>
#include <oblo/editor/services/update_dispatcher.hpp>
#include <oblo/editor/ui/style.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/editor/windows/asset_browser.hpp>
#include <oblo/editor/windows/console_window.hpp>
#include <oblo/editor/windows/editor_window.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/log/log.hpp>
#include <oblo/log/log_module.hpp>
#include <oblo/log/sinks/file_sink.hpp>
#include <oblo/log/sinks/win32_debug_sink.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/modules/utility/provider_service.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/options/options_provider.hpp>
#include <oblo/project/project.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/resource/utility/registration.hpp>
#include <oblo/runtime/runtime_module.hpp>
#include <oblo/runtime/runtime_registry.hpp>
#include <oblo/scene/resources/texture.hpp>
#include <oblo/scene/scene_editor_module.hpp>
#include <oblo/thread/job_manager.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

#include <module_loader_asset.gen.hpp>
#include <module_loader_editor.gen.hpp>

namespace oblo::editor
{
    namespace
    {
        class editor_log_sink final : public log::log_sink
        {
        public:
            void sink(log::severity severity, time timestamp, cstring_view message) override
            {
                m_logQueue.push(severity, m_baseTime - timestamp, message);
            }

            void set_base_time(time baseTime)
            {
                m_baseTime = baseTime;
            }

            log_queue& get_log_queue()
            {
                return m_logQueue;
            }

        private:
            log_queue m_logQueue;
            time m_baseTime{};
        };

        log_queue* init_log(time bootTime)
        {
            auto& mm = module_manager::get();

            auto* const logModule = mm.load<oblo::log::log_module>();

            {
                auto fileSink = allocate_unique<log::file_sink>(stderr);
                fileSink->set_base_time(bootTime);
                logModule->add_sink(std::move(fileSink));
            }

            if constexpr (platform::is_windows())
            {
                auto win32Sink = allocate_unique<log::win32_debug_sink>();
                win32Sink->set_base_time(bootTime);
                logModule->add_sink(std::move(win32Sink));
            }

            auto logSink = allocate_unique<editor_log_sink>();
            logSink->set_base_time(bootTime);
            auto& queue = logSink->get_log_queue();
            logModule->add_sink(std::move(logSink));

            return &queue;
        }

        class options_layer_helper final : public options_layer_provider
        {
            static constexpr uuid layer_uuid = "dc217469-e387-48cf-ad5a-7b6cd4a8b1fc"_uuid;

        public:
            void init()
            {
                auto& mm = module_manager::get();

                auto* const options = mm.find<options_module>();

                auto& optionsManager = options->manager();

                m_layer = optionsManager.find_layer(layer_uuid);
                m_changeId = optionsManager.get_change_id(m_layer);
                m_options = &optionsManager;
            }

            void refresh_change_id()
            {
                m_changeId = m_options->get_change_id(m_layer);
            }

            void save()
            {
                data_document doc;
                doc.init();

                m_options->store_layer(doc, doc.get_root(), m_layer);

                string_builder optionsDir;
                filesystem::parent_path(m_optionsFilePath, optionsDir);

                filesystem::create_directories(optionsDir.as<string_view>()).assert_value();

                if (!json::write(doc, m_optionsFilePath))
                {
                    log::error("Failed to write editor options to {}", m_optionsFilePath);
                }
            }

            void update()
            {
                if (const auto changeId = m_options->get_change_id(m_layer); changeId != m_changeId)
                {
                    save();
                    m_changeId = changeId;
                }
            }

            void set_options_file_path(string_view optionsFilePath)
            {
                m_optionsFilePath = optionsFilePath;
            }

        private:
            void fetch(deque<options_layer_provider_descriptor>& out) const override
            {
                out.push_back({
                    .layer = {.id = layer_uuid},
                    .load =
                        [](data_document& doc, any& userdata)
                    {
                        const string* const filePath = userdata.as<string>();

                        if (!filePath)
                        {
                            OBLO_ASSERT(false);
                            return false;
                        }

                        if (!json::read(doc, *filePath))
                        {
                            log::error("Failed to read editor options from {}", *filePath);
                            return false;
                        }

                        return true;
                    },
                    .userdata = any{m_optionsFilePath},
                });
            }

        private:
            options_manager* m_options{};
            h32<options_layer> m_layer{};
            u32 m_changeId{};
            string m_optionsFilePath;
        };

        class editor_app_module final : public module_interface
        {
        public:
            bool startup(const module_initializer& initializer) override
            {
                auto& mm = module_manager::get();

                mm.load<options_module>();
                mm.load<runtime_module>();
                mm.load<reflection::reflection_module>();
                mm.load<importers::importers_module>();
                mm.load<editor_module>();
                mm.load<scene_editor_module>();

                initializer.services->add<options_layer_provider>().externally_owned(&m_editorOptions);

                constexpr auto addRepositories = [](deque<module_repository_descriptor>& outRepositories)
                {
                    outRepositories.push_back({
                        .name = "oblo",
                        .assetsDirectory = "./data/oblo/assets",
                        .sourcesDirectory = "./data/oblo/sources",
                        .flags = asset_repository_flags::omit_import_source_path |
                            asset_repository_flags::wait_until_processed,
                    });
                };

                initializer.services->add<lambda_module_repository_provider<decltype(addRepositories)>>()
                    .as<module_repository_provider>()
                    .unique();

                gen::load_modules_asset();
                gen::load_modules_editor();

                return true;
            }

            void shutdown() override {}

            bool finalize() override
            {
                m_editorOptions.init();
                m_editorOptions.refresh_change_id();
                return true;
            }

            void update()
            {
                m_editorOptions.update();
            }

            void configure_project(string_view projectDir, project project)
            {
                m_projectDir = projectDir;
                m_project = std::move(project);

                string_builder projectOptionsDir;
                projectOptionsDir.assign(m_projectDir).append_path("options").append_path("oblo.editor.json");

                m_editorOptions.set_options_file_path(projectOptionsDir.view());
            }

            const project& get_project() const
            {
                return m_project;
            }

            cstring_view get_project_directory() const
            {
                return m_projectDir;
            }

        private:
            options_layer_helper m_editorOptions;
            project m_project;
            string_builder m_projectDir;
        };
    }

    struct app::impl
    {
        run_config m_runConfig;
        module_manager m_moduleManager;
        log_queue* m_logQueue{};
        job_manager m_jobManager;
        window_manager m_windowManager;
        asset_registry m_assetRegistry;
        editor_app_module* m_editorModule{};
        vk::vulkan_engine_module* m_vkEngine{};
        asset_editor_manager* m_assetEditors{};
        input_queue m_inputQueue;
        runtime_registry m_runtimeRegistry;
        update_dispatcher m_updateDispatcher;
        editor_window* m_mainWindow{};

        expected<> init(const run_config& cfg);
        expected<> startup();
        void startup_ui();
        void update_runtime();
        void update_registries();
        void update_ui();
        void shutdown();
    };

    app::app() = default;

    app::~app()
    {
        shutdown();
    }

    expected<> app::init(const run_config& cfg)
    {
        m_impl = allocate_unique<impl>();

        const expected e = m_impl->init(cfg);

        if (!e)
        {
            m_impl->shutdown();
            m_impl.reset();
            return e;
        }

        return m_impl->startup();
    }

    void app::shutdown()
    {
        if (m_impl)
        {
            m_impl->shutdown();
            m_impl.reset();
        }
    }

    namespace
    {
        void setup_icon(const resource_registry& registry, graphics_window& window)
        {
            constexpr resource_ref<texture> icon{"4130f5a7-12c2-e913-ac2c-c0bc8228dbec"_uuid};

            const resource_ptr ptr = registry.get_resource(icon);
            OBLO_ASSERT(ptr);

            if (ptr)
            {
                ptr.load_sync();

                const auto& desc = ptr->get_description();
                std::span<const byte> data = ptr->get_data(0, 0, 0);

                OBLO_ASSERT(desc.vkFormat == texture_format::r8g8b8a8_unorm);

                window.set_icon(desc.width, desc.height, data);
            }
        }
    }

    void app::run()
    {
        imgui_app app;

        string_builder imguiIni;
        imguiIni.assign(m_impl->m_runConfig.appDir).append_path("ui");

        if (!filesystem::create_directories(imguiIni.view()))
        {
            log::debug("Failed to create directories {}", imguiIni);
        }

        imguiIni.append_path("editor.imgui.ini");

        if (!app.init(
                {
                    .title = "oblo",
                    .style = window_style::app,
                    .isHidden = true,
                    .isMaximized = true,
                    .isBorderless = true,
                },
                m_impl->m_runtimeRegistry.get_resource_registry(),
                m_impl->m_vkEngine->get_frame_graph(),
                {
                    .configFile = imguiIni.c_str(),
                    .useMultiViewport = true,
                    .useDocking = true,
                    .useKeyboardNavigation = true,
                }))
        {
            return;
        }

        // Before accessing any resource (run a first update to make sure they are available)
        m_impl->update_registries();

        init_ui();

        m_impl->startup_ui();

        auto& globalRegistry = m_impl->m_windowManager.get_global_service_registry();
        globalRegistry.add<ImGuiContext>().externally_owned(app.get_imgui_context());

        auto& mainWindow = app.get_main_window();

        setup_icon(m_impl->m_runtimeRegistry.get_resource_registry(), mainWindow);

        // We pass this function by reference to main window, so it needs to live on the stack together with the
        // imgui_app itself
        auto hitTest = [this, &mainWindow](const vec2u& position)
        {
            constexpr i32 borderSize = 2;

            hit_test_result r = hit_test_result::normal;

            const bool isMaximized = mainWindow.is_maximized();
            const auto [w, h] = mainWindow.get_size();

            if (!isMaximized)
            {
                if (position.x <= borderSize && position.y <= borderSize)
                {
                    r = hit_test_result::resize_top_left;
                }
                else if (position.x >= w - borderSize && position.y <= borderSize)
                {
                    r = hit_test_result::resize_top_right;
                }
                else if (position.x <= borderSize && position.y >= h - borderSize)
                {
                    r = hit_test_result::resize_bottom_left;
                }
                else if (position.x >= w - borderSize && position.y >= h - borderSize)
                {
                    r = hit_test_result::resize_bottom_right;
                }
                else if (position.y <= borderSize)
                {
                    r = hit_test_result::resize_top;
                }
                else if (position.y >= h - borderSize)
                {
                    r = hit_test_result::resize_bottom;
                }
                else if (position.x <= borderSize)
                {
                    r = hit_test_result::resize_left;
                }
                else if (position.x >= w - borderSize)
                {
                    r = hit_test_result::resize_right;
                }
                else if (m_impl->m_mainWindow->is_draggable_space(position))
                {
                    r = hit_test_result::draggable;
                }
            }
            else if (m_impl->m_mainWindow->is_draggable_space(position))
            {
                r = hit_test_result::draggable;
            }

            return r;
        };

        mainWindow.set_custom_hit_test(hitTest);
        mainWindow.set_hidden(false);

        app.set_input_queue(&m_impl->m_inputQueue);

        for (; OBLO_PROFILE_FRAME_BEGIN(); OBLO_PROFILE_FRAME_END())
        {
            m_impl->m_mainWindow->set_is_maximized(mainWindow.is_maximized());

            if (!app.process_events())
            {
                break;
            }

            if (!app.acquire_images())
            {
                continue;
            }

            app.begin_ui();
            m_impl->update_ui();
            app.end_ui();

            m_impl->update_registries();
            m_impl->update_runtime();

            app.present();

            m_impl->m_inputQueue.clear();

            switch (m_impl->m_mainWindow->get_last_window_event())
            {
            default:
                break;

            case editor_window_event::minimize:
                mainWindow.minimize();
                break;
            case editor_window_event::maximize:
                mainWindow.maximize();
                break;
            case editor_window_event::restore:
                mainWindow.restore();
                break;

            case editor_window_event::close:
                // We should check if we want to save the current scene first, maybe by triggering closure of every
                // asset editor. Currenty we don't do that on ALT+F4 either though.
                return;
            }
        }
    }

    expected<> app::impl::init(const run_config& cfg)
    {
        const auto bootTime = clock::now();

        debug_assert_hook_install();

        m_runConfig = cfg;
        m_jobManager.init();

        m_logQueue = init_log(bootTime);

        log::info("Starting up oblo");
        log::info("Application data is in {}", cfg.appDir);
        log::info("Loading project from {}", cfg.projectPath);

        auto& mm = m_moduleManager;

        m_editorModule = mm.load<editor_app_module>();

        {
            expected project = project_load(cfg.projectPath);

            if (!project)
            {
                return "Failed to load project"_err;
            }

            string_builder projectDir;
            filesystem::parent_path(cfg.projectPath, projectDir);

            m_editorModule->configure_project(projectDir.view(), std::move(*project));
        }

        for (const auto& module : m_editorModule->get_project().modules)
        {
            if (!mm.load(module))
            {
                log::error("Failed to load project module: '{}'", module);
            }
        }

        m_vkEngine = mm.load<vk::vulkan_engine_module>();

        if (!mm.finalize())
        {
            return "Failed to finalize module manager"_err;
        }

        return no_error;
    }

    expected<> app::impl::startup()
    {
        auto& mm = m_moduleManager;

        const auto& project = m_editorModule->get_project();
        const auto& projectDir = m_editorModule->get_project_directory();

        string_builder assetsDir, artifactsDir, sourcesDir;
        assetsDir.append(projectDir).append_path(project.assetsDir);
        artifactsDir.append(projectDir).append_path(project.artifactsDir);
        sourcesDir.append(projectDir).append_path(project.sourcesDir);

        deque<module_repository_descriptor> moduleRepositories;

        for (auto* const provider : mm.find_services<module_repository_provider>())
        {
            provider->fetch(moduleRepositories);
        }

        buffered_array<asset_repository_descriptor, 8> assetRepositories = {
            {
                .name = "assets",
                .assetsDirectory = assetsDir,
                .sourcesDirectory = sourcesDir,
            },
        };

        assetRepositories.reserve(moduleRepositories.size() + 1);

        for (auto& repo : moduleRepositories)
        {
            assetRepositories.emplace_back() = {
                .name = hashed_string_view{repo.name},
                .assetsDirectory = repo.assetsDirectory,
                .sourcesDirectory = repo.sourcesDirectory,
                .flags = repo.flags,
            };
        }

        if (!m_assetRegistry.initialize(assetRepositories, artifactsDir))
        {
            return "Failed to initialize asset registry"_err;
        }

        m_runtimeRegistry = mm.find<runtime_module>()->create_runtime_registry();
        auto& resourceRegistry = m_runtimeRegistry.get_resource_registry();

        register_native_asset_types(m_assetRegistry, mm.find_services<native_asset_provider>());
        register_file_importers(m_assetRegistry, mm.find_services<file_importers_provider>());
        register_resource_types(resourceRegistry, mm.find_services<resource_types_provider>());

        m_assetRegistry.discover_assets(
            asset_discovery_flags::reprocess_dirty | asset_discovery_flags::garbage_collect);

        auto* const resourceProvider = m_assetRegistry.initialize_resource_provider();

        resourceRegistry.register_provider(resourceProvider);

        return no_error;
    }

    void app::impl::startup_ui()
    {
        auto* const options = m_moduleManager.find<oblo::options_module>();
        auto* const reflection = m_moduleManager.find<oblo::reflection::reflection_module>();

        m_windowManager.init();

        auto& globalRegistry = m_windowManager.get_global_service_registry();

        globalRegistry.add<gpu::gpu_instance>().externally_owned(&m_vkEngine->get_gpu_instance());
        globalRegistry.add<renderer>().externally_owned(&m_vkEngine->get_renderer());
        globalRegistry.add<frame_graph>().externally_owned(&m_vkEngine->get_frame_graph());
        globalRegistry.add<const resource_registry>().externally_owned(&m_runtimeRegistry.get_resource_registry());
        globalRegistry.add<asset_registry>().externally_owned(&m_assetRegistry);
        globalRegistry.add<const property_registry>().externally_owned(&m_runtimeRegistry.get_property_registry());
        globalRegistry.add<const reflection::reflection_registry>().externally_owned(&reflection->get_registry());
        globalRegistry.add<const input_queue>().externally_owned(&m_inputQueue);
        globalRegistry.add<component_factory>().unique();
        globalRegistry.add<const log_queue>().externally_owned(m_logQueue);
        globalRegistry.add<options_manager>().externally_owned(&options->manager());
        globalRegistry.add<registered_commands>().unique();
        globalRegistry.add<incremental_id_pool>().unique();
        globalRegistry.add<update_subscriptions>().externally_owned(&m_updateDispatcher);
        auto* const assetEditorManager = globalRegistry.add<asset_editor_manager>().unique(m_assetRegistry);

        string_builder temporaryDir;
        temporaryDir.append(m_editorModule->get_project_directory()).append_path(".oblo").append_path(".temp");

        auto* editorDirectories = globalRegistry.add<editor_directories>().unique();
        editorDirectories->init(temporaryDir).assert_value();

        const window_handle editorWindow = m_windowManager.create_window<editor_window>(service_registry{});
        m_mainWindow = m_windowManager.try_access<editor_window>(editorWindow);

        // Add all asset editors under the editor window, to make sure they are dockable
        assetEditorManager->set_window_root(editorWindow);

        m_windowManager.create_child_window<asset_browser>(editorWindow);
        m_windowManager.create_child_window<console_window>(editorWindow);

        deque<service_provider_descriptor> serviceRegistrants;

        for (auto* const provider : module_manager::get().find_services<service_provider>())
        {
            serviceRegistrants.clear();
            provider->fetch(serviceRegistrants);

            for (const auto& serviceRegistrant : serviceRegistrants)
            {
                serviceRegistrant.registerServices(globalRegistry);
            }
        }
    }

    void app::impl::shutdown()
    {
        // Shutdown all registries first
        m_windowManager.shutdown();
        m_assetRegistry.shutdown();
        m_runtimeRegistry.shutdown();

        module_manager::get().shutdown();

        m_jobManager.shutdown();

        debug_assert_hook_remove();
    }

    void app::impl::update_runtime()
    {
        OBLO_PROFILE_SCOPE();
        m_updateDispatcher.dispatch();
    }

    void app::impl::update_registries()
    {
        OBLO_PROFILE_SCOPE();

        m_assetRegistry.update();
        m_runtimeRegistry.get_resource_registry().update();
    }

    void app::impl::update_ui()
    {
        OBLO_PROFILE_SCOPE();

        m_logQueue->flush();

        m_windowManager.update();
        m_editorModule->update();
    }
}