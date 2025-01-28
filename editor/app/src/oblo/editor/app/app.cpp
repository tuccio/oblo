#include "app.hpp"

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importers/importers_module.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/asset/utility/registration.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/editor_module.hpp>
#include <oblo/editor/providers/service_provider.hpp>
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
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/resource/utility/registration.hpp>
#include <oblo/runtime/runtime_module.hpp>
#include <oblo/sandbox/context.hpp>
#include <oblo/scene/scene_editor_module.hpp>
#include <oblo/thread/job_manager.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/required_features.hpp>

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
    }

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

        void finalize() override
        {
            m_editorOptions.init();
            m_editorOptions.refresh_change_id();
        }

        void update()
        {
            m_editorOptions.update();
        }

    private:
        options_layer_helper m_editorOptions;
    };

    std::span<const char* const> app::get_required_instance_extensions() const
    {
        return module_manager::get().find<runtime_module>()->get_required_renderer_features().instanceExtensions;
    }

    VkPhysicalDeviceFeatures2 app::get_required_physical_device_features() const
    {
        return module_manager::get().find<runtime_module>()->get_required_renderer_features().physicalDeviceFeatures;
    }

    void* app::get_required_device_features() const
    {
        return module_manager::get().find<runtime_module>()->get_required_renderer_features().deviceFeaturesChain;
    }

    std::span<const char* const> app::get_required_device_extensions() const
    {
        return module_manager::get().find<runtime_module>()->get_required_renderer_features().deviceExtensions;
    }

    bool app::init()
    {
        const auto bootTime = clock::now();

        if (!platform::init())
        {
            return false;
        }

        debug_assert_hook_install();

        m_jobManager.init();

        m_logQueue = init_log(bootTime);

        auto& mm = module_manager::get();
        m_editorModule = mm.load<editor_app_module>();

        mm.finalize();

        return true;
    }

    bool app::startup(const vk::sandbox_startup_context& ctx)
    {
        auto& mm = module_manager::get();
        auto* const reflection = mm.find<oblo::reflection::reflection_module>();
        auto* const runtime = mm.find<oblo::runtime_module>();
        auto* const options = mm.find<oblo::options_module>();

        m_runtimeRegistry = runtime->create_runtime_registry();

        // TODO (#41): Load a project instead
        if (!m_assetRegistry.initialize("./project/assets", "./project/artifacts", "./project/sources"))
        {
            return false;
        }

        auto& propertyRegistry = m_runtimeRegistry.get_property_registry();
        auto& resourceRegistry = m_runtimeRegistry.get_resource_registry();

        register_native_asset_types(m_assetRegistry, mm.find_services<native_asset_provider>());
        register_file_importers(m_assetRegistry, mm.find_services<file_importers_provider>());
        register_resource_types(resourceRegistry, mm.find_services<resource_types_provider>());

        m_assetRegistry.discover_assets(asset_discovery_flags::reprocess_dirty);

        resourceRegistry.register_provider(m_assetRegistry.initialize_resource_provider());

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
        init_ui();

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
            globalRegistry.add<const log_queue>().externally_owned(m_logQueue);
            globalRegistry.add<options_manager>().externally_owned(&options->manager());
            globalRegistry.add<registered_commands>().unique();
            globalRegistry.add<incremental_id_pool>().unique();

            service_registry sceneRegistry{};
            sceneRegistry.add<ecs::entity_registry>().externally_owned(&m_runtime.get_entity_registry());

            const auto sceneEditingWindow =
                m_windowManager.create_window<scene_editing_window>(std::move(sceneRegistry));

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

        m_lastFrameTime = clock::now();

        return true;
    }

    void app::shutdown(const vk::sandbox_shutdown_context&)
    {
        m_windowManager.shutdown();
        m_runtime.shutdown();
        platform::shutdown();

        module_manager::get().shutdown();

        m_jobManager.shutdown();

        debug_assert_hook_remove();
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
        m_assetRegistry.update();
        m_runtimeRegistry.get_resource_registry().update();

        m_logQueue->flush();

        m_windowManager.update();
        m_editorModule->update();
    }
}