#include "app.hpp"

#include <oblo/app/imgui_app.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importers/importers_module.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/asset/utility/registration.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/editor_module.hpp>
#include <oblo/editor/providers/service_provider.hpp>
#include <oblo/editor/services/asset_editor_manager.hpp>
#include <oblo/editor/services/component_factory.hpp>
#include <oblo/editor/services/incremental_id_pool.hpp>
#include <oblo/editor/services/log_queue.hpp>
#include <oblo/editor/services/registered_commands.hpp>
#include <oblo/editor/ui/style.hpp>
#include <oblo/editor/windows/asset_browser.hpp>
#include <oblo/editor/windows/console_window.hpp>
#include <oblo/editor/windows/inspector.hpp>
#include <oblo/editor/windows/scene_editing_window.hpp>
#include <oblo/editor/windows/scene_hierarchy.hpp>
#include <oblo/editor/windows/style_window.hpp>
#include <oblo/editor/windows/viewport.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/log/log.hpp>
#include <oblo/log/log_module.hpp>
#include <oblo/log/sinks/file_sink.hpp>
#include <oblo/log/sinks/win32_debug_sink.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/modules/utility/provider_service.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/options/options_provider.hpp>
#include <oblo/project/project.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/resource/utility/registration.hpp>
#include <oblo/runtime/runtime_module.hpp>
#include <oblo/scene/scene_editor_module.hpp>
#include <oblo/thread/job_manager.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

#include <cxxopts.hpp>

namespace oblo
{
    std::istream& operator>>(std::istream& is, string_builder& s);
}

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
            static constexpr cstring_view options_path = "oblo_options.json";

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

                if (!json::write(doc, options_path))
                {
                    log::error("Failed to write editor options to {}", options_path);
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

            void fetch(deque<options_layer_provider_descriptor>& out) const override
            {
                out.push_back({
                    .layer = {.id = layer_uuid},
                    .load =
                        [](data_document& doc)
                    {
                        if (!json::read(doc, options_path))
                        {
                            log::error("Failed to read editor options from {}", options_path);
                            return false;
                        }

                        return true;
                    },
                });
            }

        private:
            options_manager* m_options{};
            h32<options_layer> m_layer{};
            u32 m_changeId{};
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

            expected<> parse_cli_options(int argc, char* argv[]);

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
        module_manager m_moduleManager;
        log_queue* m_logQueue{};
        job_manager m_jobManager;
        window_manager m_windowManager;
        runtime m_runtime;
        asset_registry m_assetRegistry;
        time_stats m_timeStats{};
        time m_lastFrameTime{};
        editor_app_module* m_editorModule{};
        vk::vulkan_engine_module* m_vkEngine{};
        input_queue m_inputQueue;
        runtime_registry m_runtimeRegistry;

        bool init(int argc, char* argv[]);
        bool startup();
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

    bool app::init(int argc, char* argv[])
    {
        m_impl = allocate_unique<impl>();

        if (!m_impl->init(argc, argv))
        {
            m_impl->shutdown();
            m_impl.reset();
            return false;
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

    void app::run()
    {
        imgui_app app;

        if (!app.init({.title = "oblo", .isHidden = true}, {.configFile = "oblo.imgui.ini"}))
        {
            return;
        }

        init_ui();
        m_impl->startup_ui();

        if (!app.init_font_atlas(m_impl->m_runtimeRegistry.get_resource_registry()))
        {
            return;
        }

        auto& mainWindow = app.get_main_window();

        mainWindow.maximize();
        mainWindow.set_hidden(false);

        app.set_input_queue(&m_impl->m_inputQueue);

        for (; OBLO_PROFILE_FRAME_BEGIN(); OBLO_PROFILE_FRAME_END())
        {
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
        }
    }

    bool app::impl::init(int argc, char* argv[])
    {
        const auto bootTime = clock::now();

        if (!platform::init())
        {
            return false;
        }

        debug_assert_hook_install();

        m_jobManager.init();

        m_logQueue = init_log(bootTime);

        auto& mm = m_moduleManager;
        m_editorModule = mm.load<editor_app_module>();

        if (!m_editorModule->parse_cli_options(argc, argv))
        {
            log::error("Failed to parse CLI options");
            return false;
        }

        for (const auto& module : m_editorModule->get_project().modules)
        {
            if (!mm.load(module))
            {
                log::error("Failed to load project module: '{}'", module);
            }
        }

        m_vkEngine = mm.load<vk::vulkan_engine_module>();

        return mm.finalize();
    }

    bool app::impl::startup()
    {
        auto& mm = m_moduleManager;
        auto* const reflection = mm.find<oblo::reflection::reflection_module>();

        const auto& project = m_editorModule->get_project();
        const auto& projectDir = m_editorModule->get_project_directory();

        string_builder assetsDir, artifactsDir, sourcesDir;
        assetsDir.append(projectDir).append_path(project.assetsDir);
        artifactsDir.append(projectDir).append_path(project.artifactsDir);
        sourcesDir.append(projectDir).append_path(project.sourcesDir);

        if (!m_assetRegistry.initialize(assetsDir, artifactsDir, sourcesDir))
        {
            return false;
        }

        m_runtimeRegistry = mm.find<runtime_module>()->create_runtime_registry();

        auto& propertyRegistry = m_runtimeRegistry.get_property_registry();
        auto& resourceRegistry = m_runtimeRegistry.get_resource_registry();

        register_native_asset_types(m_assetRegistry, mm.find_services<native_asset_provider>());
        register_file_importers(m_assetRegistry, mm.find_services<file_importers_provider>());
        register_resource_types(resourceRegistry, mm.find_services<resource_types_provider>());

        m_assetRegistry.discover_assets(
            asset_discovery_flags::reprocess_dirty | asset_discovery_flags::garbage_collect);

        auto* const resourceProvider = m_assetRegistry.initialize_resource_provider();

        resourceRegistry.register_provider(resourceProvider);

        if (!m_runtime.init({
                .reflectionRegistry = &reflection->get_registry(),
                .propertyRegistry = &propertyRegistry,
                .resourceRegistry = &resourceRegistry,
                .worldBuilders = mm.find_services<ecs::world_builder>(),
            }))
        {
            log::error("Failed to initialize runtime");
            return false;
        }

        m_lastFrameTime = clock::now();

        return true;
    }

    void app::impl::startup_ui()
    {
        auto* const options = m_moduleManager.find<oblo::options_module>();
        auto* const reflection = m_moduleManager.find<oblo::reflection::reflection_module>();

        m_windowManager.init();

        auto& globalRegistry = m_windowManager.get_global_service_registry();

        globalRegistry.add<vk::vulkan_context>().externally_owned(&m_vkEngine->get_vulkan_context());
        globalRegistry.add<vk::renderer>().externally_owned(&m_vkEngine->get_renderer());
        globalRegistry.add<const resource_registry>().externally_owned(&m_runtimeRegistry.get_resource_registry());
        globalRegistry.add<asset_registry>().externally_owned(&m_assetRegistry);
        globalRegistry.add<const property_registry>().externally_owned(&m_runtimeRegistry.get_property_registry());
        globalRegistry.add<const reflection::reflection_registry>().externally_owned(&reflection->get_registry());
        globalRegistry.add<const input_queue>().externally_owned(&m_inputQueue);
        globalRegistry.add<component_factory>().unique();
        globalRegistry.add<const time_stats>().externally_owned(&m_timeStats);
        globalRegistry.add<const log_queue>().externally_owned(m_logQueue);
        globalRegistry.add<options_manager>().externally_owned(&options->manager());
        globalRegistry.add<registered_commands>().unique();
        globalRegistry.add<incremental_id_pool>().unique();
        globalRegistry.add<asset_editor_manager>().unique();

        service_registry sceneRegistry{};
        sceneRegistry.add<ecs::entity_registry>().externally_owned(&m_runtime.get_entity_registry());
        sceneRegistry.add<scene_renderer>().externally_owned(m_runtime.get_service_registry().find<scene_renderer>());

        const auto sceneEditingWindow = m_windowManager.create_window<scene_editing_window>(std::move(sceneRegistry));

        m_windowManager.create_child_window<asset_browser>(sceneEditingWindow);
        m_windowManager.create_child_window<inspector>(sceneEditingWindow);
        m_windowManager.create_child_window<scene_hierarchy>(sceneEditingWindow);
        m_windowManager.create_child_window<viewport>(sceneEditingWindow);
        m_windowManager.create_child_window<console_window>(sceneEditingWindow);

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
        m_windowManager.shutdown();
        m_runtime.shutdown();
        platform::shutdown();

        module_manager::get().shutdown();

        m_jobManager.shutdown();

        debug_assert_hook_remove();
    }

    void app::impl::update_runtime()
    {
        OBLO_PROFILE_SCOPE();

        const auto now = clock::now();
        const auto dt = now - m_lastFrameTime;

        m_timeStats.dt = dt;
        m_runtime.update({.dt = dt});

        m_lastFrameTime = now;
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

    expected<> editor_app_module::parse_cli_options(int argc, char* argv[])
    {
        try
        {
            cxxopts::Options options("oblo");

            options.add_options()("project", "The path to the project file", cxxopts::value<string_builder>());

            auto r = options.parse(argc, argv);

            if (r.count("project"))
            {
                const auto& projectPath = r["project"].as<string_builder>();

                auto p = project_load(projectPath);

                if (!p)
                {
                    return unspecified_error;
                }

                m_project = *std::move(p);
                filesystem::parent_path(projectPath.view(), m_projectDir);
            }
            else
            {
                m_project.name = "New Project";
                m_project.assetsDir = "./assets";
                m_project.artifactsDir = "./.artifacts";
                m_project.sourcesDir = "./sources";
                m_projectDir = "./project";
            }

            return no_error;
        }
        catch (...)
        {
            return unspecified_error;
        }
    }
}

namespace oblo
{
    std::istream& operator>>(std::istream& is, string_builder& s)
    {
        for (int c = is.get(); c != EOF; c = is.get())
        {
            s.append(char(c));
        }

        is.clear();

        return is;
    }
}