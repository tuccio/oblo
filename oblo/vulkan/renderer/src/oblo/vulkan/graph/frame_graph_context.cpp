#include <oblo/vulkan/graph/frame_graph_context.hpp>

#include <oblo/core/flags.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/iterator/flags_range.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/log/log.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/descriptor_set_pool.hpp>
#include <oblo/vulkan/draw/shader_stage_utils.hpp>
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
        // We only support the global TLAS for now, acceleration structures are not proper resources yet
        constexpr resource<acceleration_structure> g_globalTLAS{1u};

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

            case texture_usage::storage_read:
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

        struct buffer_access_info
        {
            VkPipelineStageFlags2 pipeline;
            VkAccessFlags2 access;
            buffer_access_kind accessKind;
        };

        buffer_access_info convert_for_sync2(pass_kind passKind, buffer_usage usage)
        {
            VkPipelineStageFlags2 pipelineStage{};
            VkAccessFlags2 access{};
            buffer_access_kind accessKind{};

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
                accessKind = buffer_access_kind::read;
                break;
            case buffer_usage::storage_write: // We interpret write as RW (e.g. we may read uploaded data)
                access = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT |
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
                accessKind = buffer_access_kind::write;
                break;
            case buffer_usage::storage_upload:
                access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                accessKind = buffer_access_kind::write;
                break;
            case buffer_usage::uniform:
                access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT;
                accessKind = buffer_access_kind::read;
                break;
            case buffer_usage::indirect:
                access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
                accessKind = buffer_access_kind::read;
                break;
            default:
                unreachable();
            }

            return {pipelineStage, access, accessKind};
        }

        void add_texture_usages(
            resource_pool& resourcePool, frame_graph_impl& frameGraph, resource<texture> texture, texture_usage usage)
        {
            switch (usage)
            {
            case texture_usage::transfer_destination:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT);
                break;

            case texture_usage::transfer_source:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
                break;

            case texture_usage::shader_read:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    VK_IMAGE_USAGE_SAMPLED_BIT);
                break;

            case texture_usage::storage_read:
            case texture_usage::storage_write:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    VK_IMAGE_USAGE_STORAGE_BIT);
                break;

            case texture_usage::render_target_write:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
                break;

            default:
                break;
            }
        }

        template <typename R>
        h32<frame_graph_pin_storage> as_storage_handle(resource<R> h)
        {
            return h32<frame_graph_pin_storage>{h.value};
        }

        template <typename R>
        const R& access_storage(const frame_graph_impl& frameGraph, resource<R> h)
        {
            const auto storage = as_storage_handle(h);
            const auto* ptr = static_cast<R*>(frameGraph.access_storage(storage));
            OBLO_ASSERT(ptr);
            return *ptr;
        }

        [[maybe_unused]] bool check_buffer_usage(const frame_graph_impl& frameGraph,
            h32<frame_graph_pass> currentPass,
            h32<frame_graph_pin_storage> h,
            bool isReadOnly)
        {
            const auto& p = frameGraph.passes[currentPass.value];

            for (u32 i = p.bufferUsageBegin; i < p.bufferUsageEnd; ++i)
            {
                const auto& bufferUsage = frameGraph.bufferUsages[i];

                if (bufferUsage.pinStorage != h)
                {
                    continue;
                }

                const auto readOnlyAccess = bufferUsage.accessKind == buffer_access_kind::read;

                return readOnlyAccess == isReadOnly;
            }

            return false;
        }

        void bind_descriptor_sets(const frame_graph_impl& frameGraph,
            renderer& renderer,
            VkCommandBuffer commandBuffer,
            VkPipelineBindPoint bindPoint,
            [[maybe_unused]] h32<frame_graph_pass> currentPass,
            const base_pipeline& pipeline,
            const image_layout_tracker& imageLayoutTracker,
            const binding_tables_span& bindingTables)
        {
            const auto& pm = renderer.get_pass_manager();
            const auto& drawRegistry = renderer.get_draw_registry();
            const auto& interner = renderer.get_string_interner();

            pm.bind_descriptor_sets(commandBuffer,
                bindPoint,
                pipeline,
                [&frameGraph,
                    &pm,
                    &drawRegistry,
                    &pipeline,
                    currentPass,
                    bindingTables = bindingTables.span(),
                    &interner,
                    &imageLayoutTracker](const descriptor_binding& binding) -> bindable_object
                {
                    const hashed_string_view str = hashed_string_view{interner.str(binding.name)};

                    for (const auto& bindingTable : bindingTables)
                    {
                        auto* const r = bindingTable->try_find(str);

                        if (!r)
                        {
                            continue;
                        }

                        switch (r->kind)
                        {
                        case bindable_resource_kind::acceleration_structure: {
                            OBLO_ASSERT(r->accelerationStructure == g_globalTLAS,
                                "Only the global TLAS is supported at the moment");

                            return make_bindable_object(drawRegistry.get_tlas());
                        }

                        case bindable_resource_kind::buffer: {
                            const buffer& b = access_storage(frameGraph, r->buffer);

#if OBLO_DEBUG
                            const bool isReadOnlyBuffer =
                                binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || binding.readOnly;

                            if (!check_buffer_usage(frameGraph,
                                    currentPass,
                                    as_storage_handle(r->buffer),
                                    isReadOnlyBuffer))
                            {
                                log::error("[{}] Missing or mismatching acquire for buffer {}",
                                    pm.get_pass_name(pipeline),
                                    str);
                            }

#endif
                            return make_bindable_object(b);
                        }

                        case bindable_resource_kind::texture: {
                            const texture& t = access_storage(frameGraph, r->texture);

                            // The frame graph converts the pin storage handle to texture handle to use when keeping
                            // track of textures
                            const auto storage = as_storage_handle(r->texture);

                            const auto layout = imageLayoutTracker.try_get_layout(storage);
                            layout.assert_value();

                            return make_bindable_object(t.view, layout.value_or(VK_IMAGE_LAYOUT_UNDEFINED));
                        }

                        default:
                            unreachable();
                        }
                    }

                    return {};
                });
        }
    }

    void frame_graph_build_context::create(
        resource<texture> texture, const texture_resource_initializer& initializer, texture_usage usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

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
            .debugLabel = initializer.debugLabel,
        };

        // TODO: (#29) Reuse and alias texture memory
        constexpr lifetime_range range{0, 0};

        h32<stable_texture_resource> stableId{};

        if (initializer.isStable)
        {
            // We use the resource handle as id, since it's unique and stable as long as graph topology doesn't change
            stableId = std::bit_cast<h32<stable_texture_resource>>(texture);
        }

        const auto poolIndex = m_resourcePool.add_transient_texture(imageInitializer, range, stableId);

        m_frameGraph.add_transient_resource(texture, poolIndex);
        m_frameGraph.add_resource_transition(texture, usage);

        add_texture_usages(m_resourcePool, m_frameGraph, texture, usage);
    }

    void frame_graph_build_context::create(
        resource<buffer> buffer, const buffer_resource_initializer& initializer, buffer_usage usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        auto vkUsage = convert_buffer_usage(usage);

        if (!initializer.data.empty())
        {
            vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        h32<stable_buffer_resource> stableId{};

        if (initializer.isStable)
        {
            // We use the resource handle as id, since it's unique and stable as long as graph topology doesn't change
            stableId = std::bit_cast<h32<stable_buffer_resource>>(buffer);

            OBLO_ASSERT(initializer.data.empty(),
                "Uploading at initialization time on stable buffers is currently not supported");
        }

        const auto poolIndex = m_resourcePool.add_transient_buffer(initializer.size, vkUsage, stableId);

        staging_buffer_span stagedData{};
        staging_buffer_span* stagedDataPtr{};

        const bool upload = !initializer.data.empty();

        if (upload)
        {
            [[maybe_unused]] const auto res = m_renderer.get_staging_buffer().stage(initializer.data);
            OBLO_ASSERT(res, "Out of space on the staging buffer, we should flush instead");

            stagedData = *res;
            stagedDataPtr = &stagedData;

            // We rely on a global memory barrier in frame graph to synchronize all uploads before submitting any
            // command
        }

        m_frameGraph.add_transient_buffer(buffer, poolIndex, stagedDataPtr);

        const auto& currentPass = m_frameGraph.passes[m_state.currentPass.value];

        const auto [pipelineStage, access, accessKind] = convert_for_sync2(currentPass.kind, usage);
        m_frameGraph.set_buffer_access(buffer, pipelineStage, access, accessKind, upload);
    }

    void frame_graph_build_context::create(
        resource<buffer> buffer, const staging_buffer_span& stagedData, buffer_usage usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        auto vkUsage = convert_buffer_usage(usage);

        const auto stagedDataSize = calculate_size(stagedData);

        if (stagedDataSize != 0)
        {
            vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        constexpr h32<stable_buffer_resource> notStable{};

        const auto poolIndex = m_resourcePool.add_transient_buffer(stagedDataSize, vkUsage, notStable);

        // We rely on a global memory barrier in frame graph to synchronize all uploads before submitting any command

        m_frameGraph.add_transient_buffer(buffer, poolIndex, &stagedData);

        const auto& currentPass = m_frameGraph.passes[m_state.currentPass.value];

        const auto [pipelineStage, access, accessKind] = convert_for_sync2(currentPass.kind, usage);
        m_frameGraph.set_buffer_access(buffer, pipelineStage, access, accessKind, stagedDataSize != 0);
    }

    void frame_graph_build_context::acquire(resource<texture> texture, texture_usage usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        m_frameGraph.add_resource_transition(texture, usage);
        add_texture_usages(m_resourcePool, m_frameGraph, texture, usage);
    }

    h32<resident_texture> frame_graph_build_context::acquire_bindless(resource<texture> texture,
        texture_usage usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        m_frameGraph.add_resource_transition(texture, usage);
        add_texture_usages(m_resourcePool, m_frameGraph, texture, usage);

        const auto bindlessHandle = m_renderer.get_texture_registry().acquire();
        m_frameGraph.bindlessTextures.emplace_back(bindlessHandle, texture, usage);

        return bindlessHandle;
    }

    h32<resident_texture> frame_graph_build_context::load_resource(const resource_ptr<oblo::texture>& texture) const
    {
        return m_renderer.get_resource_cache().get_or_add(texture);
    }

    void frame_graph_build_context::acquire(resource<buffer> buffer, buffer_usage usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        const auto poolIndex = m_frameGraph.find_pool_index(buffer);
        OBLO_ASSERT(poolIndex, "The buffer might not have an input connected, or needs to be created");
        m_resourcePool.add_transient_buffer_usage(poolIndex, convert_buffer_usage(usage));

        const auto& currentPass = m_frameGraph.passes[m_state.currentPass.value];
        const auto [pipelineStage, access, accessKind] = convert_for_sync2(currentPass.kind, usage);

        m_frameGraph.set_buffer_access(buffer, pipelineStage, access, accessKind, false);
    }

    void frame_graph_build_context::reroute(resource<buffer> source, resource<buffer> destination) const
    {
        m_frameGraph.reroute(source, destination);
    }

    bool frame_graph_build_context::has_source(resource<buffer> buffer) const
    {
        auto* const owner = m_frameGraph.get_owner_node(buffer);
        return m_frameGraph.currentNode != owner;
    }

    bool frame_graph_build_context::has_source(resource<texture> texture) const
    {
        auto* const owner = m_frameGraph.get_owner_node(texture);
        return m_frameGraph.currentNode != owner;
    }

    resource<buffer> frame_graph_build_context::create_dynamic_buffer(const buffer_resource_initializer& initializer,
        buffer_usage usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        const auto pinHandle = m_frameGraph.allocate_dynamic_resource_pin();

        const resource<buffer> resource{pinHandle.value};
        create(resource, initializer, usage);

        return resource;
    }

    resource<buffer> frame_graph_build_context::create_dynamic_buffer(const staging_buffer_span& stagedData,
        buffer_usage usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        const auto pinHandle = m_frameGraph.allocate_dynamic_resource_pin();

        const resource<buffer> resource{pinHandle.value};
        create(resource, stagedData, usage);

        return resource;
    }

    expected<image_initializer> frame_graph_build_context::get_current_initializer(resource<texture> texture) const
    {
        const auto h = m_frameGraph.find_pool_index(texture);

        if (!h)
        {
            return unspecified_error;
        }

        return m_resourcePool.get_initializer(h);
    }

    frame_allocator& frame_graph_build_context::get_frame_allocator() const
    {
        return m_frameGraph.dynamicAllocator;
    }

    const draw_registry& frame_graph_build_context::get_draw_registry() const
    {
        return m_renderer.get_draw_registry();
    }

    ecs::entity_registry& frame_graph_build_context::get_entity_registry() const
    {
        return m_renderer.get_draw_registry().get_entity_registry();
    }

    random_generator& frame_graph_build_context::get_random_generator() const
    {
        return m_frameGraph.rng;
    }

    staging_buffer_span frame_graph_build_context::stage_upload(std::span<const byte> data) const
    {
        return m_renderer.get_staging_buffer().stage(data).value();
    }

    frame_graph_build_context::frame_graph_build_context(
        frame_graph_impl& frameGraph, frame_graph_build_state& state, renderer& renderer, resource_pool& resourcePool) :
        m_frameGraph{frameGraph}, m_state{state}, m_renderer{renderer}, m_resourcePool{resourcePool}
    {
    }

    h32<frame_graph_pass> frame_graph_build_context::begin_pass(pass_kind kind) const
    {
        return m_frameGraph.begin_pass_build(m_state, kind);
    }

    h32<compute_pass_instance> frame_graph_build_context::compute_pass(h32<vk::compute_pass> pass,
        const compute_pipeline_initializer& initializer) const
    {
        const auto h = m_frameGraph.begin_pass_build(m_state, pass_kind::compute);
        auto& pm = m_renderer.get_pass_manager();
        m_frameGraph.passes[h.value].computePipeline = pm.get_or_create_pipeline(pass, initializer);

        return h32<compute_pass_instance>{h.value};
    }

    h32<render_pass_instance> frame_graph_build_context::render_pass(h32<vk::render_pass> pass,
        const render_pipeline_initializer& initializer) const
    {
        const auto h = m_frameGraph.begin_pass_build(m_state, pass_kind::graphics);
        auto& pm = m_renderer.get_pass_manager();
        m_frameGraph.passes[h.value].renderPipeline = pm.get_or_create_pipeline(pass, initializer);

        return h32<render_pass_instance>{h.value};
    }

    h32<raytracing_pass_instance> frame_graph_build_context::raytracing_pass(h32<vk::raytracing_pass> pass,
        const raytracing_pipeline_initializer& initializer) const
    {
        const auto h = m_frameGraph.begin_pass_build(m_state, pass_kind::raytracing);
        auto& pm = m_renderer.get_pass_manager();
        m_frameGraph.passes[h.value].raytracingPipeline = pm.get_or_create_pipeline(pass, initializer);

        return h32<raytracing_pass_instance>{h.value};
    }

    void* frame_graph_build_context::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        return m_frameGraph.access_storage(handle);
    }

    bool frame_graph_build_context::has_event_impl(const type_id& type) const
    {
        return m_frameGraph.emptyEvents.contains(type);
    }

    frame_graph_execute_context::frame_graph_execute_context(const frame_graph_impl& frameGraph,
        frame_graph_execution_state& executeCtx,
        renderer& renderer,
        VkCommandBuffer commandBuffer) :
        m_frameGraph{frameGraph}, m_state{executeCtx}, m_renderer{renderer}, m_commandBuffer{commandBuffer}
    {
    }

    void frame_graph_execute_context::begin_pass(h32<frame_graph_pass> handle) const
    {
        m_frameGraph.begin_pass_execution(handle, m_commandBuffer, m_state);
    }

    expected<> frame_graph_execute_context::begin_pass(h32<compute_pass_instance> handle) const
    {
        OBLO_ASSERT(handle);
        OBLO_ASSERT(m_frameGraph.passes[handle.value].kind == pass_kind::compute);

        const auto passHandle = h32<frame_graph_pass>{handle.value};
        m_frameGraph.begin_pass_execution(passHandle, m_commandBuffer, m_state);

        auto& pm = get_pass_manager();

        const auto pipeline = m_frameGraph.passes[handle.value].computePipeline;
        const auto computeCtx = pm.begin_compute_pass(m_commandBuffer, pipeline);

        if (!computeCtx)
        {
            // Do we need to do anything?
            m_state.passKind = pass_kind::none;
            return unspecified_error;
        }

        m_state.passKind = pass_kind::compute;
        m_state.computeCtx = *computeCtx;
        m_state.basePipeline = pm.get_base_pipeline(computeCtx->internalPipeline);

        return no_error;
    }

    expected<> frame_graph_execute_context::begin_pass(h32<render_pass_instance> handle,
        const VkRenderingInfo& renderingInfo) const
    {
        OBLO_ASSERT(handle);
        OBLO_ASSERT(m_frameGraph.passes[handle.value].kind == pass_kind::graphics);

        const auto passHandle = h32<frame_graph_pass>{handle.value};
        m_frameGraph.begin_pass_execution(passHandle, m_commandBuffer, m_state);

        auto& pm = get_pass_manager();

        const auto pipeline = m_frameGraph.passes[handle.value].renderPipeline;
        const auto renderCtx = pm.begin_render_pass(m_commandBuffer, pipeline, renderingInfo);

        if (!renderCtx)
        {
            // Do we need to do anything?
            m_state.passKind = pass_kind::none;
            return unspecified_error;
        }

        m_state.passKind = pass_kind::graphics;
        m_state.renderCtx = *renderCtx;
        m_state.basePipeline = pm.get_base_pipeline(renderCtx->internalPipeline);

        return no_error;
    }

    expected<> frame_graph_execute_context::begin_pass(h32<raytracing_pass_instance> handle) const
    {
        OBLO_ASSERT(handle);
        OBLO_ASSERT(m_frameGraph.passes[handle.value].kind == pass_kind::raytracing);

        const auto passHandle = h32<frame_graph_pass>{handle.value};
        m_frameGraph.begin_pass_execution(passHandle, m_commandBuffer, m_state);

        auto& pm = get_pass_manager();

        const auto pipeline = m_frameGraph.passes[handle.value].raytracingPipeline;
        const auto rtCtx = pm.begin_raytracing_pass(m_commandBuffer, pipeline);

        if (!rtCtx)
        {
            // Do we need to do anything?
            m_state.passKind = pass_kind::none;
            return unspecified_error;
        }

        m_state.passKind = pass_kind::raytracing;
        m_state.rtCtx = *rtCtx;
        m_state.basePipeline = pm.get_base_pipeline(rtCtx->internalPipeline);

        return no_error;
    }

    void frame_graph_execute_context::end_pass() const
    {
        auto& pm = get_pass_manager();

        switch (m_state.passKind)
        {
        case pass_kind::compute:
            pm.end_compute_pass(m_state.computeCtx);
            break;

        case pass_kind::graphics:
            pm.end_render_pass(m_state.renderCtx);
            break;

        case pass_kind::raytracing:
            pm.end_raytracing_pass(m_state.rtCtx);
            break;

        default:
            OBLO_ASSERT(false);
            break;
        }
    }

    void frame_graph_execute_context::bind_descriptor_sets(binding_tables_span bindingTables) const
    {
        VkPipelineBindPoint bindPoint;

        switch (m_state.passKind)
        {
        case pass_kind::raytracing:
            bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
            break;
        case pass_kind::graphics:
            bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            break;
        case pass_kind::compute:
            bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
            break;
        default:
            unreachable();
        }

        vk::bind_descriptor_sets(m_frameGraph,
            m_renderer,
            m_commandBuffer,
            bindPoint,
            m_state.currentPass,
            *m_state.basePipeline,
            m_state.imageLayoutTracker,
            bindingTables);
    }

    texture frame_graph_execute_context::access(resource<texture> h) const
    {
        return vk::access_storage(m_frameGraph, h);
    }

    buffer frame_graph_execute_context::access(resource<buffer> h) const
    {
        return vk::access_storage(m_frameGraph, h);
    }

    resource<acceleration_structure> frame_graph_execute_context::get_global_tlas() const
    {
        return g_globalTLAS;
    }

    bool frame_graph_execute_context::has_source(resource<buffer> buffer) const
    {
        auto* const owner = m_frameGraph.get_owner_node(buffer);
        return m_frameGraph.currentNode != owner;
    }

    bool frame_graph_execute_context::has_source(resource<texture> texture) const
    {
        auto* const owner = m_frameGraph.get_owner_node(texture);
        return m_frameGraph.currentNode != owner;
    }

    u32 frame_graph_execute_context::get_frames_alive_count(resource<texture> texture) const
    {
        const auto h = m_frameGraph.find_pool_index(texture);
        OBLO_ASSERT(h);
        return m_frameGraph.resourcePool.get_frames_alive_count(h);
    }

    u32 frame_graph_execute_context::get_frames_alive_count(resource<buffer> buffer) const
    {
        const auto h = m_frameGraph.find_pool_index(buffer);
        OBLO_ASSERT(h);
        return m_frameGraph.resourcePool.get_frames_alive_count(h);
    }

    u32 frame_graph_execute_context::get_current_frames_count() const
    {
        return m_frameGraph.frameCounter;
    }

    void frame_graph_execute_context::upload(resource<buffer> h, std::span<const byte> data, u32 bufferOffset) const
    {
        OBLO_ASSERT(m_state.currentPass && m_frameGraph.passes[m_state.currentPass.value].kind == pass_kind::transfer);

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

    void frame_graph_execute_context::upload(
        resource<buffer> h, const staging_buffer_span& data, u32 bufferOffset) const
    {
        auto& stagingBuffer = m_renderer.get_staging_buffer();
        const auto b = access(h);
        stagingBuffer.upload(get_command_buffer(), data, b.buffer, b.offset + bufferOffset);
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

            // The frame graph converts the pin storage handle to texture handle to use when keeping track of textures
            const auto storage = h32<frame_graph_pin_storage>{b.resource.value};

            const auto layout = m_state.imageLayoutTracker.try_get_layout(storage);
            layout.assert_value();

            table.emplace(interner.get_or_add(b.name),
                make_bindable_object(texture.view, layout.value_or(VK_IMAGE_LAYOUT_UNDEFINED)));
        }
    }

    const gpu_info& frame_graph_execute_context::get_gpu_info() const
    {
        return m_frameGraph.gpuInfo;
    }

    void frame_graph_execute_context::push_constants(
        flags<shader_stage> stages, u32 offset, std::span<const byte> bytes) const
    {
        auto& pm = get_pass_manager();

        VkShaderStageFlags vkShaderFlags{};

        for (const auto flag : flags_range{stages})
        {
            vkShaderFlags |= to_vk_shader_stage(flag);
        }

        pm.push_constants(m_commandBuffer, *m_state.basePipeline, vkShaderFlags, offset, bytes);
    }

    void frame_graph_execute_context::dispatch_compute(u32 groupsX, u32 groupsY, u32 groupsZ) const
    {
        OBLO_ASSERT(m_state.passKind == pass_kind::compute);
        vkCmdDispatch(m_commandBuffer, groupsX, groupsY, groupsZ);
    }

    void frame_graph_execute_context::trace_rays(u32 x, u32 y, u32 z) const
    {
        OBLO_ASSERT(m_state.passKind == pass_kind::raytracing);
        get_pass_manager().trace_rays(m_state.rtCtx, x, y, z);
    }

    void* frame_graph_execute_context::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        return m_frameGraph.access_storage(handle);
    }

    bool frame_graph_execute_context::has_event_impl(const type_id& type) const
    {
        return m_frameGraph.emptyEvents.contains(type);
    }

    frame_graph_init_context::frame_graph_init_context(frame_graph_impl& frameGraph, renderer& renderer) :
        m_frameGraph{frameGraph}, m_renderer{renderer}
    {
    }

    pass_manager& frame_graph_init_context::get_pass_manager() const
    {
        return m_renderer.get_pass_manager();
    }

    string_interner& frame_graph_init_context::get_string_interner() const
    {
        return m_renderer.get_string_interner();
    }

    h32<compute_pass> frame_graph_init_context::register_compute_pass(const compute_pass_initializer& initializer) const
    {
        return m_renderer.get_pass_manager().register_compute_pass(initializer);
    }

    h32<render_pass> frame_graph_init_context::register_render_pass(const render_pass_initializer& initializer) const
    {
        return m_renderer.get_pass_manager().register_render_pass(initializer);
    }

    h32<raytracing_pass> frame_graph_init_context::register_raytracing_pass(
        const raytracing_pass_initializer& initializer) const
    {
        return m_renderer.get_pass_manager().register_raytracing_pass(initializer);
    }
}