#include <oblo/vulkan/graph/frame_graph_context.hpp>

#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/graph/frame_graph_impl.hpp>
#include <oblo/vulkan/graph/resource_pool.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/staging_buffer.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    namespace
    {
        VkImageUsageFlags convert_usage(texture_usage usage)
        {
            switch (usage)
            {
            case texture_usage::depth_stencil_read:
            case texture_usage::depth_stencil_write:
                return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

            case texture_usage::render_target_write:
                return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            case texture_usage::shader_read:
                return VK_IMAGE_USAGE_SAMPLED_BIT;

            case texture_usage::storage_write:
                return {};

            default:
                OBLO_ASSERT(false);
                return {};
            };
        }

        VkBufferUsageFlags convert_buffer_usage(buffer_usage usage)
        {
            VkBufferUsageFlags result{};

            OBLO_ASSERT(usage != buffer_usage::enum_max);

            switch (usage)
            {
            case buffer_usage::storage_read:
            case buffer_usage::storage_write:
            case buffer_usage::storage_upload:
                result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                break;

            case buffer_usage::indirect:
                result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
                break;

            case buffer_usage::uniform:
                result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                break;

            default:
                unreachable();
            }

            return result;
        }

        std::pair<VkPipelineStageFlags2, VkAccessFlags2> convert_for_sync2(pass_kind passKind, buffer_usage usage)
        {
            VkPipelineStageFlags2 pipelineStage{};
            VkAccessFlags2 access{};

            switch (passKind)
            {
            case pass_kind::none:
                pipelineStage = VK_PIPELINE_STAGE_2_NONE;
                break;

            case pass_kind::graphics:
                pipelineStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
                break;

            case pass_kind::compute:
                pipelineStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                break;

            case pass_kind::raytracing:
                pipelineStage = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
                break;

            case pass_kind::transfer:
                pipelineStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                break;

            default:
                unreachable();
            }

            switch (usage)
            {
            case buffer_usage::storage_read:
                access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
                break;
            case buffer_usage::storage_write:
                access = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
                break;
            case buffer_usage::storage_upload:
                access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                break;
            case buffer_usage::uniform:
                access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT;
                break;
            case buffer_usage::indirect:
                access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
                break;
            default:
                unreachable();
            }

            return {pipelineStage, access};
        }

        void add_texture_usages(
            resource_pool& resourcePool, frame_graph_impl& frameGraph, resource<texture> texture, texture_usage usage)
        {
            switch (usage)
            {
            case texture_usage::transfer_destination:
                resourcePool.add_usage(frameGraph.find_pool_index(texture), VK_IMAGE_USAGE_TRANSFER_DST_BIT);
                break;

            case texture_usage::transfer_source:
                resourcePool.add_usage(frameGraph.find_pool_index(texture), VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
                break;

            case texture_usage::shader_read:
                resourcePool.add_usage(frameGraph.find_pool_index(texture), VK_IMAGE_USAGE_SAMPLED_BIT);
                break;

            case texture_usage::storage_read:
            case texture_usage::storage_write:
                resourcePool.add_usage(frameGraph.find_pool_index(texture), VK_IMAGE_USAGE_STORAGE_BIT);
                break;

            default:
                break;
            }
        }
    }

    void frame_graph_build_context::create(
        resource<texture> texture, const transient_texture_initializer& initializer, texture_usage usage) const
    {
        const image_initializer imageInitializer{
            .imageType = VK_IMAGE_TYPE_2D,
            .format = initializer.format,
            .extent = {.width = initializer.width, .height = initializer.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = convert_usage(usage) | VK_IMAGE_USAGE_SAMPLED_BIT | initializer.usage,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .memoryUsage = memory_usage::gpu_only,
        };

        // TODO: (#29) Reuse and alias texture memory
        constexpr lifetime_range range{0, 0};

        const auto poolIndex = m_resourcePool.add(imageInitializer, range);
        m_frameGraph.add_transient_resource(texture, poolIndex);
        m_frameGraph.add_resource_transition(texture, usage);

        add_texture_usages(m_resourcePool, m_frameGraph, texture, usage);
    }

    void frame_graph_build_context::create(resource<buffer> buffer,
        const transient_buffer_initializer& initializer,
        pass_kind passKind,
        buffer_usage usage) const
    {
        auto vkUsage = convert_buffer_usage(usage);

        if (!initializer.data.empty())
        {
            vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        const auto poolIndex = m_resourcePool.add_buffer(initializer.size, vkUsage);

        staging_buffer_span stagedData{};
        staging_buffer_span* stagedDataPtr{};

        if (!initializer.data.empty())
        {
            [[maybe_unused]] const auto res = m_renderer.get_staging_buffer().stage(initializer.data);
            OBLO_ASSERT(res, "Out of space on the staging buffer, we should flush instead");

            stagedData = *res;
            stagedDataPtr = &stagedData;

            // We rely on a global memory barrier in frame graph to synchronize all uploads before submitting any
            // command
        }

        m_frameGraph.add_transient_buffer(buffer, poolIndex, stagedDataPtr);

        const auto [pipelineStage, access] = convert_for_sync2(passKind, usage);
        m_frameGraph.set_buffer_access(buffer, pipelineStage, access);
    }

    void frame_graph_build_context::create(
        resource<buffer> buffer, const staging_buffer_span& stagedData, pass_kind passKind, buffer_usage usage) const
    {
        auto vkUsage = convert_buffer_usage(usage);

        const auto stagedDataSize = calculate_size(stagedData);

        if (stagedDataSize != 0)
        {
            vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        const auto poolIndex = m_resourcePool.add_buffer(stagedDataSize, vkUsage);

        // We rely on a global memory barrier in frame graph to synchronize all uploads before submitting any command

        m_frameGraph.add_transient_buffer(buffer, poolIndex, &stagedData);

        const auto [pipelineStage, access] = convert_for_sync2(passKind, usage);
        m_frameGraph.set_buffer_access(buffer, pipelineStage, access);
    }

    void frame_graph_build_context::acquire(resource<texture> texture, texture_usage usage) const
    {
        m_frameGraph.add_resource_transition(texture, usage);
        add_texture_usages(m_resourcePool, m_frameGraph, texture, usage);
    }

    h32<resident_texture> frame_graph_build_context::acquire_bindless(resource<texture> texture,
        texture_usage usage) const
    {
        m_frameGraph.add_resource_transition(texture, usage);
        add_texture_usages(m_resourcePool, m_frameGraph, texture, usage);

        const auto bindlessHandle = m_renderer.get_texture_registry().acquire();
        m_frameGraph.bindlessTextures.emplace_back(bindlessHandle, texture, usage);

        return bindlessHandle;
    }

    void frame_graph_build_context::acquire(resource<buffer> buffer, pass_kind passKind, buffer_usage usage) const
    {
        const auto poolIndex = m_frameGraph.find_pool_index(buffer);
        m_resourcePool.add_buffer_usage(poolIndex, convert_buffer_usage(usage));
        const auto [pipelineStage, access] = convert_for_sync2(passKind, usage);
        m_frameGraph.set_buffer_access(buffer, pipelineStage, access);
    }

    resource<buffer> frame_graph_build_context::create_dynamic_buffer(
        const transient_buffer_initializer& initializer, pass_kind passKind, buffer_usage usage) const
    {
        const auto pinHandle = m_frameGraph.allocate_dynamic_resource_pin();

        const resource<buffer> resource{pinHandle.value};
        create(resource, initializer, passKind, usage);

        return resource;
    }

    resource<buffer> frame_graph_build_context::create_dynamic_buffer(
        const staging_buffer_span& stagedData, pass_kind passKind, buffer_usage usage) const
    {
        const auto pinHandle = m_frameGraph.allocate_dynamic_resource_pin();

        const resource<buffer> resource{pinHandle.value};
        create(resource, stagedData, passKind, usage);

        return resource;
    }

    frame_allocator& frame_graph_build_context::get_frame_allocator() const
    {
        return m_frameGraph.dynamicAllocator;
    }

    const draw_registry& frame_graph_build_context::get_draw_registry() const
    {
        return m_renderer.get_draw_registry();
    }
    frame_graph_build_context::frame_graph_build_context(
        frame_graph_impl& frameGraph, renderer& renderer, resource_pool& resourcePool) :
        m_frameGraph{frameGraph},
        m_renderer{renderer}, m_resourcePool{resourcePool}
    {
    }

    void* frame_graph_build_context::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        return m_frameGraph.access_storage(handle);
    }

    frame_graph_execute_context::frame_graph_execute_context(
        frame_graph_impl& frameGraph, renderer& renderer, VkCommandBuffer commandBuffer) :
        m_frameGraph{frameGraph},
        m_renderer{renderer}, m_commandBuffer{commandBuffer}
    {
    }

    texture frame_graph_execute_context::access(resource<texture> h) const
    {
        const auto storage = h32<frame_graph_pin_storage>{h.value};
        const auto* texturePtr = static_cast<texture*>(m_frameGraph.access_storage(storage));

        OBLO_ASSERT(texturePtr);
        return *texturePtr;
    }

    buffer frame_graph_execute_context::access(resource<buffer> h) const
    {
        const auto storage = h32<frame_graph_pin_storage>{h.value};
        return *static_cast<buffer*>(m_frameGraph.access_storage(storage));
    }

    void frame_graph_execute_context::upload(resource<buffer> h, std::span<const byte> data, u32 bufferOffset) const
    {
        OBLO_ASSERT(m_frameGraph.can_exec_time_upload(h));

        auto& stagingBuffer = m_renderer.get_staging_buffer();
        const auto stagedData = stagingBuffer.stage(data);

        if (!stagedData)
        {
            OBLO_ASSERT(stagedData);
            return;
        }

        const auto b = access(h);
        stagingBuffer.upload(get_command_buffer(), *stagedData, b.buffer, b.offset + bufferOffset);
    }

    VkCommandBuffer frame_graph_execute_context::get_command_buffer() const
    {
        return m_commandBuffer;
    }

    VkDevice frame_graph_execute_context::get_device() const
    {
        return m_renderer.get_vulkan_context().get_device();
    }

    pass_manager& frame_graph_execute_context::get_pass_manager() const
    {
        return m_renderer.get_pass_manager();
    }

    draw_registry& frame_graph_execute_context::get_draw_registry() const
    {
        return m_renderer.get_draw_registry();
    }

    string_interner& frame_graph_execute_context::get_string_interner() const
    {
        return m_renderer.get_string_interner();
    }

    const loaded_functions& frame_graph_execute_context::get_loaded_functions() const
    {
        return m_renderer.get_vulkan_context().get_loaded_functions();
    }

    void frame_graph_execute_context::bind_buffers(binding_table& table,
        std::initializer_list<buffer_binding_desc> bindings) const
    {
        auto& interner = get_string_interner();

        for (const auto& b : bindings)
        {
            table.emplace(interner.get_or_add(b.name), make_bindable_object(access(b.resource)));
        }
    }

    void frame_graph_execute_context::bind_textures(binding_table& table,
        std::initializer_list<texture_binding_desc> bindings) const
    {
        auto& interner = get_string_interner();

        for (const auto& b : bindings)
        {
            const auto& texture = access(b.resource);
            table.emplace(interner.get_or_add(b.name), make_bindable_object(texture.view));
        }
    }

    void* frame_graph_execute_context::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        return m_frameGraph.access_storage(handle);
    }

    frame_graph_init_context::frame_graph_init_context(renderer& renderer) : m_renderer{renderer} {}

    pass_manager& frame_graph_init_context::get_pass_manager() const
    {
        return m_renderer.get_pass_manager();
    }

    string_interner& frame_graph_init_context::get_string_interner() const
    {
        return m_renderer.get_string_interner();
    }
}