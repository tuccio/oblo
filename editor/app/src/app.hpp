#pragma once

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/systems/system_seq_executor.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/editor/window_manager.hpp>

namespace oblo::vk
{
    struct sandbox_init_context;
    struct sandbox_shutdown_context;
    struct sandbox_render_context;
    struct sandbox_update_imgui_context;

    class stateful_command_buffer;
}

namespace oblo::editor
{
    class app
    {
    public:
        bool init(const vk::sandbox_init_context& context);

        void shutdown(const vk::sandbox_shutdown_context& context);

        void update(const vk::sandbox_render_context& context);

        void update_imgui(const vk::sandbox_update_imgui_context& context);

    private:
        window_manager m_windowManager;
        ecs::system_seq_executor m_executor;
        ecs::type_registry m_typeRegistry;
        ecs::entity_registry m_entities;
        service_registry m_services;
        vk::stateful_command_buffer* m_currentCb{};
    };
}