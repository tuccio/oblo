#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>

#include <span>
#include <unordered_map>
#include <vector>

namespace oblo::resource
{
    class resource_registry;
}

namespace oblo::vk
{
    class staging_buffer;
    class vulkan_context;
    struct buffer_column_description;
    struct draw_instance;
    struct draw_mesh;

    struct draw_buffer
    {
        std::string_view name;
        u32 elementSize;
        u32 elementAlignment;
    };

    class draw_registry
    {
    public:
        draw_registry();
        draw_registry(const draw_registry&) = delete;
        draw_registry(draw_registry&&) noexcept = delete;

        ~draw_registry();

        draw_registry& operator=(const draw_registry&) = delete;
        draw_registry& operator=(draw_registry&&) noexcept = delete;

        void init(vulkan_context& ctx, staging_buffer& stagingBuffer, string_interner& interner);
        void shutdown();

        h64<draw_mesh> get_or_create_mesh(oblo::resource::resource_registry& resourceRegistry, const uuid& resourceId);

        h32<draw_instance> create_instance(
            h64<draw_mesh> mesh, std::span<const h32<draw_buffer>> buffers, std::span<std::byte*> outData);

        void destroy_instance(h32<draw_instance> id);

        h32<draw_buffer> get_or_register(const draw_buffer& buffer);

    private:
        struct mesh_batch;
        struct mesh_batch_id;

    private:
        mesh_batch* get_or_create_mesh_batch(const mesh_batch_id& batchId,
            std::span<const buffer_column_description> unsortedAttributeNames);

    private:
        vulkan_context* m_ctx{};
        staging_buffer* m_stagingBuffer{};
        string_interner* m_interner{};
        std::vector<buffer_column_description> m_vertexAttributes;
        std::vector<mesh_batch> m_meshBatches;
        ecs::type_registry m_typeRegistry;
        ecs::entity_registry m_instances;

        std::unordered_map<uuid, h64<draw_mesh>> m_cachedMeshes;
    };
}