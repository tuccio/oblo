#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/vulkan/draw/mesh_database.hpp>
#include <oblo/vulkan/dynamic_buffer.hpp>
#include <oblo/vulkan/gpu_temporary_aliases.hpp>
#include <oblo/vulkan/monotonic_gbu_buffer.hpp>

#include <array>
#include <span>
#include <unordered_map>
#include <vector>

namespace oblo
{
    class mesh;
    class resource_registry;
    class string_interner;

    template <typename>
    struct resource_ref;
}

namespace oblo
{
    class instance_data_type_registry;
    class staging_buffer;
    class vulkan_context;
    struct draw_mesh;
    struct staging_buffer_span;

    struct draw_buffer
    {
        string_view name;
        u32 elementSize;
        u32 elementAlignment;
    };

    struct draw_instance_buffers
    {
        u32* instanceBufferIds;
        staging_buffer_span* buffersData;
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

        void init(vulkan_context& ctx,
            staging_buffer& stagingBuffer,
            string_interner& interner,
            ecs::entity_registry& entities,
            const resource_registry& resourceRegistry,
            const instance_data_type_registry& instanceDataTypeRegistry);

        void shutdown();

        h32<draw_mesh> try_get_mesh(const resource_ref<mesh>& resourceId) const;
        h32<draw_mesh> get_or_create_mesh(const resource_ref<mesh>& resourceId);

        void flush_uploads(VkCommandBuffer commandBuffer);

        void generate_mesh_database(frame_allocator& allocator);
        void generate_draw_calls(frame_allocator& allocator);
        void generate_raytracing_structures(frame_allocator& allocator, VkCommandBuffer commandBuffer);

        std::span<const batch_draw_data> get_draw_calls() const;

        std::span<const std::byte> get_mesh_database_data() const;

        VkAccelerationStructureKHR get_tlas() const;

        ecs::entity_registry& get_entity_registry() const;

    private:
        struct blas;
        struct pending_mesh_upload;
        struct instance_data_type_info;

        struct rt_acceleration_structure
        {
            VkAccelerationStructureKHR accelerationStructure;
            VkDeviceAddress deviceAddress;
            allocated_buffer buffer;
        };

    private:
        void create_instances();
        void defer_upload(const std::span<const byte> data, const buffer& b);

        void release(rt_acceleration_structure& as);

    private:
        vulkan_context* m_ctx{};

        staging_buffer* m_stagingBuffer{};
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
        std::array<h32<string>, MeshBuffersCount> m_meshDataNames{};

        dynamic_array<pending_mesh_upload> m_pendingMeshUploads;

        h32_flat_extpool_dense_map<draw_mesh, blas> m_meshToBlas;

        dynamic_buffer m_rtScratchBuffer;
        dynamic_buffer m_rtInstanceBuffer;

        rt_acceleration_structure m_tlas{};
    };
}