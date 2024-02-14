#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>

#include <span>
#include <vector>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    struct sandbox_init_context;
    struct sandbox_shutdown_context;
    struct sandbox_render_context;
    struct sandbox_update_imgui_context;

    class vertex_pull
    {
    public:
        VkPhysicalDeviceFeatures get_required_physical_device_features() const;
        void* get_device_features_list() const;

        bool init(const sandbox_init_context& context);
        void shutdown(const sandbox_shutdown_context& context);
        void update(const sandbox_render_context& context);
        void update_imgui(const sandbox_update_imgui_context& context);

    private:
        enum class method : u8
        {
            vertex_buffers,
            vertex_buffers_indirect,
            vertex_pull_indirect,
            vertex_pull_merge,
        };

    private:
        bool compile_shader_modules(frame_allocator& allocator, VkDevice device);

        bool create_pools(VkDevice device);
        bool create_descriptor_set_layouts(VkDevice device);
        bool create_pipelines(VkDevice device, VkFormat swapchainFormat);

        bool create_buffers(VkDevice device, gpu_allocator& allocator);
        bool create_descriptor_sets(VkDevice device);

        void create_geometry();

        void destroy_buffers(gpu_allocator& allocator);
        void destroy_pipelines(VkDevice device);
        void destroy_shader_modules(VkDevice device);
        void destroy_pools(VkDevice device);

        void compute_layout_params();

    private:
        static constexpr u32 MaxFramesInFlight{2};

        std::vector<allocated_buffer> m_positionBuffers;
        std::vector<allocated_buffer> m_colorBuffers;
        std::vector<allocated_buffer> m_indirectDrawBuffers;
        allocated_buffer m_mergeIndirectionBuffer{};
        allocated_buffer m_mergeIndirectDrawCommandsBuffer{};
        allocated_buffer m_positionBuffersRefs{};
        allocated_buffer m_colorBuffersRefs{};

        VkShaderModule m_shaderVertexBuffersVert{nullptr};
        VkShaderModule m_shaderVertexPullVert{nullptr};
        VkShaderModule m_shaderVertexPullMergeVert{nullptr};
        VkShaderModule m_shaderSharedFrag{nullptr};

        VkPipelineLayout m_vertexBuffersPipelineLayout{nullptr};
        VkPipelineLayout m_vertexPullPipelineLayout{nullptr};
        VkPipelineLayout m_vertexPullMergePipelineLayout{nullptr};
        VkPipeline m_vertexBuffersPipeline{nullptr};
        VkPipeline m_vertexPullPipeline{nullptr};
        VkPipeline m_vertexPullMergePipeline{nullptr};

        VkDescriptorPool m_descriptorPools[MaxFramesInFlight]{nullptr};
        VkDescriptorSetLayout m_vertexPullSetLayout{nullptr};
        VkDescriptorSetLayout m_vertexPullMergeSetLayout{nullptr};

        VkQueryPool m_queryPools[2]{{}};

        std::vector<vec3> m_positions;
        std::vector<vec3> m_colors;
        std::vector<VkDrawIndirectCommand> m_indirectDrawCommands;
        std::vector<VkDrawIndirectCommand> m_mergeIndirectDrawCommands;
        std::vector<u32> m_mergeIndirection;

        u64 m_lastRecordedTime{0u};

        u8 m_enqueuedTimestamps[2]{{}};
        method m_method{method::vertex_buffers};
        u32 m_batchesCount{8u};
        u32 m_objectsPerBatch{32u};
        u32 m_verticesPerObject{3u};

        u32 m_layoutQuadsPerRow;
        f32 m_layoutQuadScale;
        f32 m_layoutOffset;

        u32 m_frameIndex{0};
    };
}