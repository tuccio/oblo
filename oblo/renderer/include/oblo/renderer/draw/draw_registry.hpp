#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/vulkan/vulkan_instance.hpp>
#include <oblo/renderer/draw/mesh_database.hpp>

#include <array>
#include <span>
#include <unordered_map>

namespace oblo
{
    class mesh;
    class resource_registry;
    class string_interner;

    template <typename>
    struct resource_ref;

    class instance_data_type_registry;
    struct draw_mesh;

    struct draw_buffer
    {
        string_view name;
        u32 elementSize;
        u32 elementAlignment;
    };

    struct draw_instance_buffers
    {
        u32* instanceBufferIds;
        gpu::staging_buffer_span* buffersData;
        u32 count;
    };

    struct batch_draw_data
    {
        draw_instance_buffers instanceBuffers;
        u32 instanceTableId;
        u32 numInstances;
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

        void init(gpu::gpu_instance& ctx,
            gpu::staging_buffer& stagingBuffer,
            string_interner& interner,
            ecs::entity_registry& entities,
            const resource_registry& resourceRegistry,
            const instance_data_type_registry& instanceDataTypeRegistry);

        void shutdown();

        h32<draw_mesh> try_get_mesh(const resource_ref<mesh>& resourceId) const;
        h32<draw_mesh> get_or_create_mesh(const resource_ref<mesh>& resourceId);

        void flush_uploads(hptr<gpu::command_buffer> commandBuffer);

        void generate_mesh_database(frame_allocator& allocator);
        void generate_draw_calls(frame_allocator& allocator);
        void generate_raytracing_structures(frame_allocator& allocator, hptr<gpu::command_buffer> commandBuffer);

        std::span<const batch_draw_data> get_draw_calls() const;

        std::span<const std::byte> get_mesh_database_data() const;

        h32<gpu::acceleration_structure> get_tlas() const;

        ecs::entity_registry& get_entity_registry() const;

    private:
        struct blas;
        struct pending_mesh_upload;
        struct instance_data_type_info;

        struct rt_acceleration_structure;
        struct rt_data;

    private:
        void create_instances();
        void defer_upload(const std::span<const byte> data, const gpu::buffer_range& b);

        void release(rt_acceleration_structure& as);

    private:
        gpu::gpu_instance* m_ctx{};
        gpu::vk::vulkan_instance* m_vk{};

        gpu::staging_buffer* m_stagingBuffer{};
        const resource_registry* m_resourceRegistry{};
        mesh_database m_meshes;
        ecs::entity_registry* m_entities{};
        const ecs::type_registry* m_typeRegistry{};

        ecs::component_type m_instanceComponent{};
        ecs::component_type m_instanceIdComponent{};
        ecs::tag_type m_indexNoneTag{};
        ecs::tag_type m_indexU8Tag{};
        ecs::tag_type m_indexU16Tag{};
        ecs::tag_type m_indexU32Tag{};

        const batch_draw_data* m_drawData{};
        u32 m_drawDataCount{};

        std::span<const std::byte> m_meshDatabaseData;

        std::unordered_map<uuid, h32<draw_mesh>> m_cachedMeshes;

        flat_dense_map<ecs::component_type, instance_data_type_info> m_instanceDataTypeNames;
        ecs::type_set m_instanceDataTypes{};

        static constexpr u32 MeshBuffersCount{3};
        std::array<h32<buffer_table_name>, MeshBuffersCount> m_meshDataNames{};

        dynamic_array<pending_mesh_upload> m_pendingMeshUploads;

        h32_flat_extpool_dense_map<draw_mesh, blas> m_meshToBlas;

        unique_ptr<rt_data> m_rt;
    };
}