#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/vulkan/draw/mesh_database.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/monotonic_gbu_buffer.hpp>

#include <array>
#include <span>
#include <unordered_map>
#include <vector>

namespace oblo
{
    class mesh;
    class resource_registry;

    template <typename>
    struct resource_ref;
}

namespace oblo::vk
{
    class staging_buffer;
    class vulkan_context;
    struct buffer_column_description;
    struct draw_mesh;
    struct staging_buffer_span;

    struct draw_commands
    {
        std::span<const byte> drawCommands;
        u32 drawCount;
        bool isIndexed;
        VkBuffer indexBuffer;
        u32 indexBufferOffset;
        VkIndexType indexType;
    };

    struct draw_buffer
    {
        std::string_view name;
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
        draw_commands drawCommands;
        u32 instanceTableId;
    };

    struct draw_mesh_component
    {
        h32<draw_mesh> mesh;
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
            ecs::entity_registry& entities);

        void shutdown();

        void register_instance_data(ecs::component_type type, std::string_view instanceName);

        bool needs_reloading_instance_data_types() const;

        void end_frame();

        h32<draw_mesh> get_or_create_mesh(oblo::resource_registry& resourceRegistry,
            const resource_ref<mesh>& resourceId);

        void flush_uploads(VkCommandBuffer commandBuffer);

        void generate_mesh_database(frame_allocator& allocator);
        void generate_draw_calls(frame_allocator& allocator, staging_buffer& stagingBuffer);
        std::string_view refresh_instance_data_defines(frame_allocator& allocator);

        std::span<const batch_draw_data> get_draw_calls() const;

        std::span<const std::byte> get_mesh_database_data() const;

        void debug_log(const batch_draw_data& drawData) const;

    private:
        struct pending_mesh_upload;
        struct instance_data_type_info;

    private:
        void create_instances();

    private:
        vulkan_context* m_ctx{};
        monotonic_gpu_buffer m_storageBuffer;

        staging_buffer* m_stagingBuffer{};
        string_interner* m_interner{};
        mesh_database m_meshes;
        ecs::entity_registry* m_entities{};
        ecs::type_registry* m_typeRegistry{};

        ecs::component_type m_instanceComponent{};
        ecs::tag_type m_indexNoneTag{};
        ecs::tag_type m_indexU16Tag{};
        ecs::tag_type m_indexU32Tag{};

        const batch_draw_data* m_drawData{};
        u32 m_drawDataCount{};

        bool m_isInstanceTypeInfoDirty{};

        std::span<const std::byte> m_meshDatabaseData;

        std::unordered_map<uuid, h32<draw_mesh>> m_cachedMeshes;

        flat_dense_map<ecs::component_type, instance_data_type_info> m_instanceDataTypeNames;
        ecs::type_set m_instanceDataTypes{};

        static constexpr u32 MeshBuffersCount{1};
        std::array<h32<string>, MeshBuffersCount> m_meshDataNames{};

        dynamic_array<pending_mesh_upload> m_pendingMeshUploads;
    };
}