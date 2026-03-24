#pragma once

#include <oblo/core/uuid.hpp>
#include <oblo/resource/resource_ref.hpp>

#include <unordered_map>

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo
{
    class draw_registry;
    class resource_cache;
    class resource_registry;

    struct skin;
    struct skeleton;

    class mesh_system
    {
    public:
        mesh_system();
        mesh_system(const mesh_system&) = delete;
        mesh_system(mesh_system&&) noexcept = delete;
        ~mesh_system();

        mesh_system& operator==(const mesh_system&) = delete;
        mesh_system& operator==(mesh_system&&) noexcept = delete;

        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        struct skin_info;

    private:
        const skin_info* get_or_add_skin(resource_ref<skin> skin);

    private:
        draw_registry* m_drawRegistry{};
        const resource_registry* m_resourceRegistry;
        resource_cache* m_resourceCache;

        std::unordered_map<uuid, skin_info> m_skinInfo;
    };
}