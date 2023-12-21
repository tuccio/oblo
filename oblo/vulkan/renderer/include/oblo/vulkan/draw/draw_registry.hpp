#pragma once

#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/draw/mesh_database.hpp>
#include <oblo/vulkan/monotonic_gbu_buffer.hpp>

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
    struct draw_instance;
    struct draw_mesh;

    struct draw_commands
    {
        VkBuffer buffer;
        VkDeviceSize offset;
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
        h32<draw_buffer>* bindings;
        vk::buffer* buffers;
        u32 count;
    };

    struct batch_draw_data
    {
        draw_instance_buffers instanceBuffers;
        draw_commands drawCommands;
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

        void end_frame();

        h32<draw_mesh> get_or_create_mesh(oblo::resource_registry& resourceRegistry,
            const resource_ref<mesh>& resourceId);

        h32<draw_instance> create_instance(
            h32<draw_mesh> mesh, std::span<const h32<draw_buffer>> buffers, std::span<std::byte*> outData);

        void get_instance_data(
            h32<draw_instance> instance, std::span<const h32<draw_buffer>> buffers, std::span<std::byte*> outData);

        void destroy_instance(h32<draw_instance> id);

        h32<draw_buffer> get_or_register(const draw_buffer& buffer);
        h32<string> get_name(h32<draw_buffer> drawBuffer) const;

        void generate_mesh_database(frame_allocator& allocator, staging_buffer& stagingBuffer);
        void generate_draw_calls(frame_allocator& allocator, staging_buffer& stagingBuffer);

        std::span<const batch_draw_data> get_draw_calls() const;

        buffer get_mesh_database_buffer() const;

    private:
        vulkan_context* m_ctx{};
        monotonic_gpu_buffer m_storageBuffer;
        monotonic_gpu_buffer m_drawCallsBuffer;

        staging_buffer* m_stagingBuffer{};
        string_interner* m_interner{};
        mesh_database m_meshes;
        ecs::type_registry m_typeRegistry;
        ecs::entity_registry m_instances;

        ecs::component_type m_meshComponent{};
        ecs::tag_type m_indexNoneTag{};
        ecs::tag_type m_indexU16Tag{};
        ecs::tag_type m_indexU32Tag{};

        const batch_draw_data* m_drawData{};
        VkBuffer m_meshTablesBuffer{};
        u32 m_drawDataCount{};
        u32 m_meshTablesBufferOffset{};
        u32 m_meshTablesBufferSize{};

        std::unordered_map<uuid, h32<draw_mesh>> m_cachedMeshes;

        flat_dense_map<h32<draw_buffer>, h32<string>> m_meshNames;
    };
}