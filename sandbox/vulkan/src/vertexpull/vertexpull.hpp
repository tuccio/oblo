#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/vulkan/allocator.hpp>

#include <vector>

namespace oblo::vk
{
    struct sandbox_init_context;
    struct sandbox_shutdown_context;
    struct sandbox_render_context;

    class vertexpull
    {
    public:
        bool init(const sandbox_init_context& context);
        void shutdown(const sandbox_shutdown_context& context);
        void update(const sandbox_render_context& context);
        void update_imgui();

    private:
        enum class batch_kind
        {
            position_color,
            position_normal_color,
            position_uv0_color,
            position_uv1_color,
            position_uv0_uv1_color,
            position_uv0_normal_color,
            position_uv1_normal_color,
            position_uv0_uv1_normal_color,
            total
        };

        enum class method : u8
        {
            vertex_buffers,
            vertex_pulling,
            vertex_pulling_merge,
        };

    private:
        bool compile_shader_modules(VkDevice device);

        bool create_pipelines(VkDevice device, VkFormat swapchainFormat);

        bool create_vertex_buffers(allocator& allocator, u32 instancesPerBatch);

        void create_geometry();

        void destroy_buffers(allocator& allocator);
        void destroy_pipelines(VkDevice device);
        void destroy_shader_modules(VkDevice device);

    private:
        static constexpr u32 BatchesCount{u32(batch_kind::total)};

        allocator::buffer m_positionBuffers[BatchesCount]{{}};
        allocator::buffer m_colorBuffers[BatchesCount]{{}};

        VkShaderModule m_shaderVertexBuffersVert{nullptr};
        VkShaderModule m_shaderSharedFrag{nullptr};

        VkPipelineLayout m_pipelineLayout{nullptr};
        VkPipeline m_vertexBuffersPipeline{nullptr};

        method m_method{method::vertex_buffers};
        u32 m_objectsPerBatch{32u};

        std::vector<vec3> m_positions;
        std::vector<vec3> m_colors;
    };
}