#include <oblo/renderer/graph/frame_graph_context.hpp>

#include <oblo/core/flags.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/iterator/flags_range.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/gpu/staging_buffer.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/vulkan/utility/image_utils.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/async_download.hpp>
#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/graph/enums.hpp>
#include <oblo/renderer/graph/frame_graph_impl.hpp>
#include <oblo/renderer/graph/resource_pool.hpp>
#include <oblo/renderer/graph/types_internal.hpp>
#include <oblo/renderer/platform/renderer_platform.hpp>
#include <oblo/renderer/renderer.hpp>

namespace oblo
{
    namespace
    {
        // We only support the global TLAS for now, acceleration structures are not proper resources yet
        constexpr pin::acceleration_structure g_globalTLAS{1u};

        gpu::image_usage convert_texture_access(gpu::image_resource_state usage)
        {
            switch (usage)
            {
            case gpu::image_resource_state::depth_stencil_read:
            case gpu::image_resource_state::depth_stencil_write:
                return gpu::image_usage::depth_stencil;

            case gpu::image_resource_state::render_target_write:
                return gpu::image_usage::color_attachment;

            case gpu::image_resource_state::shader_read:
                return gpu::image_usage::shader_sample;

            case gpu::image_resource_state::transfer_destination:
                return gpu::image_usage::transfer_destination;

            case gpu::image_resource_state::transfer_source:
                return gpu::image_usage::transfer_source;

            case gpu::image_resource_state::storage_read:
            case gpu::image_resource_state::storage_write:
                return gpu::image_usage::storage;

            default:
                OBLO_ASSERT(false);
                return {};
            };
        }

        flags<gpu::image_usage> convert_texture_access(flags<gpu::image_resource_state> usages)
        {
            flags<gpu::image_usage> r{};

            for (const gpu::image_resource_state usage : flags_range{usages})
            {
                r |= convert_texture_access(usage);
            }

            return r;
        }

        gpu::buffer_usage convert_buffer_usage(buffer_access usage)
        {
            OBLO_ASSERT(usage != buffer_access::enum_max);

            switch (usage)
            {
            case buffer_access::storage_read:
            case buffer_access::storage_write:
            case buffer_access::storage_upload:
                return gpu::buffer_usage::storage;

            case buffer_access::indirect:
                return gpu::buffer_usage::indirect;

            case buffer_access::uniform:
                return gpu::buffer_usage::uniform;

            case buffer_access::download:
                return gpu::buffer_usage::transfer_source;

            case buffer_access::index:
                return gpu::buffer_usage::index;

            default:
                unreachable();
            }
        }

        gpu::image_descriptor create_image_initializer(const texture_resource_initializer& initializer,
            flags<gpu::image_usage> usageFlags)
        {
            return {
                .format = initializer.format,
                .width = initializer.width,
                .height = initializer.height,
                .depth = 1,
                .mipLevels = 1,
                .arrayLayers = 1,
                .type = gpu::image_type::plain_2d,
                .samples = gpu::samples_count::one,
                .memoryUsage = gpu::memory_usage::gpu_only,
                .usages = usageFlags,
                .debugLabel = initializer.debugLabel,
            };
        }

        struct buffer_access_info
        {
            flags<gpu::pipeline_sync_stage> pipeline;
            flags<gpu::memory_access_type> access;
            buffer_access_kind accessKind;
        };

        buffer_access_info convert_for_sync2(pass_kind passKind, buffer_access usage)
        {
            flags<gpu::pipeline_sync_stage> pipelineStage{};
            flags<gpu::memory_access_type> access{};
            buffer_access_kind accessKind{};

            switch (passKind)
            {
            case pass_kind::none:
                pipelineStage = {};
                break;

            case pass_kind::graphics:
                pipelineStage = gpu::pipeline_sync_stage::graphics;
                break;

            case pass_kind::compute:
                pipelineStage = gpu::pipeline_sync_stage::compute;
                break;

            case pass_kind::raytracing:
                pipelineStage = gpu::pipeline_sync_stage::raytracing;
                break;

            case pass_kind::transfer:
                pipelineStage = gpu::pipeline_sync_stage::transfer;
                break;

            default:
                unreachable();
            }

            switch (usage)
            {
            case buffer_access::storage_read:
                access = gpu::memory_access_type::any_read;
                accessKind = buffer_access_kind::read;
                break;
            case buffer_access::storage_write: // We interpret write as RW (e.g. we may read uploaded data)
                access = gpu::memory_access_type::any_read | gpu::memory_access_type::any_write;
                accessKind = buffer_access_kind::write;
                break;
            case buffer_access::storage_upload:
                access = gpu::memory_access_type::any_write;
                accessKind = buffer_access_kind::write;
                break;

            case buffer_access::download:
                access = gpu::memory_access_type::any_read;
                accessKind = buffer_access_kind::read;
                break;

            case buffer_access::uniform:
                access = gpu::memory_access_type::any_read;
                accessKind = buffer_access_kind::read;
                break;

            case buffer_access::indirect:
                access = gpu::memory_access_type::any_read;
                accessKind = buffer_access_kind::read;
                break;

            case buffer_access::index:
                access = gpu::memory_access_type::any_read;
                accessKind = buffer_access_kind::read;
                break;

            default:
                unreachable();
            }

            return {pipelineStage, access, accessKind};
        }

        void add_texture_accesss(resource_pool& resourcePool,
            frame_graph_impl& frameGraph,
            pin::texture texture,
            gpu::image_resource_state usage)
        {
            switch (usage)
            {
            case gpu::image_resource_state::transfer_destination:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    gpu::image_usage::transfer_destination);
                break;

            case gpu::image_resource_state::transfer_source:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    gpu::image_usage::transfer_source);
                break;

            case gpu::image_resource_state::shader_read:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    gpu::image_usage::shader_sample);
                break;

            case gpu::image_resource_state::storage_read:
            case gpu::image_resource_state::storage_write:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    gpu::image_usage::storage);
                break;

            case gpu::image_resource_state::render_target_write:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    gpu::image_usage::color_attachment);
                break;

            case gpu::image_resource_state::depth_stencil_write:
            case gpu::image_resource_state::depth_stencil_read:
                resourcePool.add_transient_texture_usage(frameGraph.find_pool_index(texture),
                    gpu::image_usage::depth_stencil);
                break;

            default:
                break;
            }
        }

        template <typename R>
        constexpr h32<frame_graph_pin_storage> as_storage_handle(pin::resource<R> h)
        {
            return h32<frame_graph_pin_storage>{h.value};
        }

        constexpr h32<frame_graph_pin_storage> as_storage_handle(h32<retained_texture> h)
        {
            return h32<frame_graph_pin_storage>{h.value};
        }

        template <typename T, typename R>
        const T& access_storage_as(const frame_graph_impl& frameGraph, pin::resource<R> h)
        {
            const auto storage = as_storage_handle(h);
            const auto* ptr = static_cast<T*>(frameGraph.access_storage(storage));
            OBLO_ASSERT(ptr);
            return *ptr;
        }

        const frame_graph_texture_impl& access_storage_resource(const frame_graph_impl& frameGraph, pin::texture h)
        {
            return access_storage_as<frame_graph_texture_impl>(frameGraph, h);
        }

        const frame_graph_buffer_impl& access_storage_resource(const frame_graph_impl& frameGraph, pin::buffer h)
        {
            return access_storage_as<frame_graph_buffer_impl>(frameGraph, h);
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

        class binding_locator
        {
        public:
            binding_locator(const frame_graph_impl& frameGraph,
                const frame_graph_execute_args& args,
                const gpu::image_state_tracker& imageStateTracker,
                const binding_tables_span& bindingTables,
                [[maybe_unused]] const base_pipeline& pipeline,
                [[maybe_unused]] h32<frame_graph_pass> currentPass) :
                m_frameGraph{frameGraph}, m_interner{args.rendererPlatform.passManager.get_string_interner()},
                m_imageStateTracker{imageStateTracker}, m_bindingTables{bindingTables}, m_executeArgs{args},
                m_pipeline{pipeline}, m_currentPass{currentPass}
            {
            }

            gpu::bindable_object operator()(const named_shader_binding& binding) const
            {
                const hashed_string_view str = m_interner.h_str(binding.name);

                for (const auto& bindingTable : m_bindingTables.span())
                {
                    auto* const r = bindingTable->try_find(str);

                    if (!r)
                    {
                        continue;
                    }

                    switch (r->kind)
                    {
                    case gpu::bindable_resource_kind::acceleration_structure: {
                        OBLO_ASSERT(r->accelerationStructure == g_globalTLAS,
                            "Only the global TLAS is supported at the moment");

                        return gpu::make_bindable_object(m_frameGraph.globalTLAS);
                    }

                    case gpu::bindable_resource_kind::buffer: {
                        const frame_graph_buffer_impl& b = access_storage_resource(m_frameGraph, r->buffer);

#if OBLO_DEBUG
                        const bool isReadOnlyBuffer =
                            binding.kind == gpu::resource_binding_kind::uniform || binding.readOnly;

                        if (!check_buffer_usage(m_frameGraph,
                                m_currentPass,
                                as_storage_handle(r->buffer),
                                isReadOnlyBuffer))
                        {
                            log::error("[{}] Missing or mismatching acquire for buffer {}",
                                m_executeArgs.rendererPlatform.passManager.get_pass_name(m_pipeline),
                                str);
                        }

#endif
                        return gpu::make_bindable_object(gpu::bindable_buffer{
                            .buffer = b.handle,
                            .offset = b.offset,
                            .size = b.size,
                        });
                    }

                    case gpu::bindable_resource_kind::image: {
                        const frame_graph_texture_impl& t = access_storage_resource(m_frameGraph, r->texture);

                        gpu::image_resource_state state = gpu::image_resource_state::undefined;

                        [[maybe_unused]] const bool hasState = m_imageStateTracker.try_get_state(t.handle, state);
                        OBLO_ASSERT(hasState);

                        return gpu::make_bindable_object(gpu::bindable_image{
                            .image = t.handle,
                            .state = state,
                        });
                    }

                    default:
                        unreachable();
                    }
                }

                return {};
            }

        private:
            const frame_graph_impl& m_frameGraph;
            const string_interner& m_interner;
            const gpu::image_state_tracker& m_imageStateTracker;
            const binding_tables_span& m_bindingTables;
            const frame_graph_execute_args& m_executeArgs;
            [[maybe_unused]] const base_pipeline& m_pipeline;
            [[maybe_unused]] h32<frame_graph_pass> m_currentPass;
        };
    }

    void frame_graph_build_context::create(
        pin::texture texture, const texture_resource_initializer& initializer, gpu::image_resource_state usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        const gpu::image_descriptor imageInitializer =
            create_image_initializer(initializer, convert_texture_access(usage) | gpu::image_usage::shader_sample);

        // TODO: (#29) Reuse and alias texture memory
        constexpr lifetime_range range{0, 0};

        h32<stable_texture_resource> stableId{};

        if (initializer.isStable)
        {
            // We use the resource handle as id, since it's unique and stable as long as graph topology doesn't change
            stableId = std::bit_cast<h32<stable_texture_resource>>(texture);
        }

        const auto poolIndex = m_frameGraph.resourcePool.add_transient_texture(imageInitializer, range, stableId);

        m_frameGraph.add_transient_resource(texture, poolIndex);
        m_frameGraph.add_resource_transition(texture, usage);

        add_texture_accesss(m_frameGraph.resourcePool, m_frameGraph, texture, usage);
    }

    void frame_graph_build_context::create(
        pin::buffer buffer, const buffer_resource_initializer& initializer, buffer_access usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        flags allUsages = convert_buffer_usage(usage);

        if (!initializer.data.empty())
        {
            allUsages |= gpu::buffer_usage::transfer_destination;
        }

        h32<stable_buffer_resource> stableId{};

        if (initializer.isStable)
        {
            // We use the resource handle as id, since it's unique and stable as long as graph topology doesn't change
            stableId = std::bit_cast<h32<stable_buffer_resource>>(buffer);

            OBLO_ASSERT(initializer.data.empty(),
                "Uploading at initialization time on stable buffers is currently not supported");
        }

        const auto poolIndex = m_frameGraph.resourcePool.add_transient_buffer(initializer.size, allUsages, stableId);

        staging_buffer_span stagedData{};
        staging_buffer_span* stagedDataPtr{};

        const bool upload = !initializer.data.empty();

        if (upload)
        {
            [[maybe_unused]] const auto res = m_args.stagingBuffer.stage(initializer.data);
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

        if (usage == buffer_access::download)
        {
            OBLO_ASSERT(currentPass.kind == pass_kind::transfer);
            m_frameGraph.add_download(buffer);
        }
    }

    void frame_graph_build_context::create(
        pin::buffer buffer, const staging_buffer_span& stagedData, buffer_access usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        flags allUsages = convert_buffer_usage(usage);

        const auto stagedDataSize = calculate_size(stagedData);

        if (stagedDataSize != 0)
        {
            allUsages |= gpu::buffer_usage::transfer_destination;
        }

        constexpr h32<stable_buffer_resource> notStable{};

        const auto poolIndex = m_frameGraph.resourcePool.add_transient_buffer(stagedDataSize, allUsages, notStable);

        // We rely on a global memory barrier in frame graph to synchronize all uploads before submitting any command

        m_frameGraph.add_transient_buffer(buffer, poolIndex, &stagedData);

        const auto& currentPass = m_frameGraph.passes[m_state.currentPass.value];

        const auto [pipelineStage, access, accessKind] = convert_for_sync2(currentPass.kind, usage);
        m_frameGraph.set_buffer_access(buffer, pipelineStage, access, accessKind, stagedDataSize != 0);

        if (usage == buffer_access::download)
        {
            OBLO_ASSERT(currentPass.kind == pass_kind::transfer);
            m_frameGraph.add_download(buffer);
        }
    }

    h32<retained_texture> frame_graph_build_context::create_retained_texture(
        const texture_resource_initializer& initializer, flags<gpu::image_resource_state> usages) const
    {
        const gpu::image_descriptor& imageInit = create_image_initializer(initializer, convert_texture_access(usages));
        const h32 storage = m_frameGraph.create_retained_texture(m_args.gpu, imageInit);
        return h32<retained_texture>{storage.value};
    }

    void frame_graph_build_context::destroy_retained_texture(h32<retained_texture> handle) const
    {
        const h32 storageHandle = as_storage_handle(handle);
        m_frameGraph.destroy_retained_texture(m_args.gpu, storageHandle);
    }

    pin::texture frame_graph_build_context::get_resource(h32<retained_texture> texture) const
    {
        return {texture.value};
    }

    void frame_graph_build_context::register_texture(pin::texture resource, h32<gpu::image> externalTexture) const
    {
        const auto poolIndex = m_frameGraph.resourcePool.add_external_texture(*m_state.gpu, externalTexture);
        m_frameGraph.add_transient_resource(resource, poolIndex);
    }

    void frame_graph_build_context::register_global_tlas(h32<gpu::acceleration_structure> accelerationStructure) const
    {
        OBLO_ASSERT(!m_frameGraph.globalTLAS);
        m_frameGraph.globalTLAS = accelerationStructure;
    }

    void frame_graph_build_context::acquire(pin::texture texture, gpu::image_resource_state usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        m_frameGraph.add_resource_transition(texture, usage);
        add_texture_accesss(m_frameGraph.resourcePool, m_frameGraph, texture, usage);
    }

    h32<resident_texture> frame_graph_build_context::acquire_bindless(pin::texture texture,
        gpu::image_resource_state usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        m_frameGraph.add_resource_transition(texture, usage);
        add_texture_accesss(m_frameGraph.resourcePool, m_frameGraph, texture, usage);

        const auto bindlessHandle = m_args.rendererPlatform.textureRegistry.acquire();
        m_frameGraph.bindlessTextures.emplace_back(bindlessHandle, texture, usage);

        return bindlessHandle;
    }

    h32<resident_texture> frame_graph_build_context::load_resource(const resource_ptr<oblo::texture>& texture) const
    {
        return m_args.rendererPlatform.resourceCache.get_or_add(texture);
    }

    void frame_graph_build_context::acquire(pin::buffer buffer, buffer_access usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        const auto poolIndex = m_frameGraph.find_pool_index(buffer);
        OBLO_ASSERT(poolIndex, "The buffer might not have an input connected, or needs to be created");
        m_frameGraph.resourcePool.add_transient_buffer_usage(poolIndex, convert_buffer_usage(usage));

        const auto& currentPass = m_frameGraph.passes[m_state.currentPass.value];
        const auto [pipelineStage, access, accessKind] = convert_for_sync2(currentPass.kind, usage);

        m_frameGraph.set_buffer_access(buffer, pipelineStage, access, accessKind, false);

        if (usage == buffer_access::download)
        {
            OBLO_ASSERT(currentPass.kind == pass_kind::transfer);
            m_frameGraph.add_download(buffer);
        }
    }

    void frame_graph_build_context::reroute(pin::buffer source, pin::buffer destination) const
    {
        // Source is a node that should end its path here
        // Destination is a node with no incoming edges, owned by the current node
        OBLO_ASSERT(m_frameGraph.get_owner_node(destination) == m_frameGraph.currentNode,
            "Only the source of the pin should reroute");

        const auto srcStorageHandle = as_storage_handle(source);
        const auto dstStorageHandle = as_storage_handle(destination);

        m_frameGraph.reroute(srcStorageHandle, dstStorageHandle);
    }

    void frame_graph_build_context::reroute(pin::texture source, pin::texture destination) const
    {
        // Source is a node that should end its path here
        // Destination is a node with no incoming edges, owned by the current node
        OBLO_ASSERT(m_frameGraph.get_owner_node(destination) == m_frameGraph.currentNode,
            "Only the source of the pin should reroute");

        const auto srcStorageHandle = as_storage_handle(source);
        const auto dstStorageHandle = as_storage_handle(destination);

        m_frameGraph.reroute(srcStorageHandle, dstStorageHandle);
    }

    bool frame_graph_build_context::is_active_output(pin::texture t) const
    {
        const auto storageHandle = as_storage_handle(t);
        auto& storage = m_frameGraph.pinStorage.at(storageHandle);
        return storage.hasPathToOutput;
    }

    bool frame_graph_build_context::has_source(pin::buffer buffer) const
    {
        auto* const owner = m_frameGraph.get_owner_node(buffer);
        return m_frameGraph.currentNode != owner;
    }

    bool frame_graph_build_context::has_source(pin::texture texture) const
    {
        auto* const owner = m_frameGraph.get_owner_node(texture);
        return m_frameGraph.currentNode != owner;
    }

    pin::buffer frame_graph_build_context::create_dynamic_buffer(const buffer_resource_initializer& initializer,
        buffer_access usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        const auto pinHandle = m_frameGraph.allocate_dynamic_resource_pin();

        const pin::buffer resource{pinHandle.value};
        create(resource, initializer, usage);

        return resource;
    }

    pin::buffer frame_graph_build_context::create_dynamic_buffer(const staging_buffer_span& stagedData,
        buffer_access usage) const
    {
        OBLO_ASSERT(m_state.currentPass);

        const auto pinHandle = m_frameGraph.allocate_dynamic_resource_pin();

        const pin::buffer resource{pinHandle.value};
        create(resource, stagedData, usage);

        return resource;
    }

    expected<texture_init_desc> frame_graph_build_context::get_current_initializer(pin::texture texture) const
    {
        const auto h = m_frameGraph.find_pool_index(texture);

        if (!h)
        {
            return "Texture resource not found in frame graph pool"_err;
        }

        const auto& init = m_frameGraph.resourcePool.get_initializer(h);

        return texture_init_desc{
            .width = init.width,
            .height = init.height,
            .format = init.format,
        };
    }

    frame_allocator& frame_graph_build_context::get_frame_allocator() const
    {
        return m_frameGraph.dynamicAllocator;
    }

    random_generator& frame_graph_build_context::get_random_generator() const
    {
        return m_frameGraph.rng;
    }

    staging_buffer_span frame_graph_build_context::stage_upload(std::span<const byte> data) const
    {
        return m_args.stagingBuffer.stage(data).value();
    }

    staging_buffer_span frame_graph_build_context::stage_upload_image(std::span<const byte> data, u32 texelSize) const
    {
        return m_args.stagingBuffer.stage_image(data, texelSize).value();
    }

    u32 frame_graph_build_context::get_current_frames_count() const
    {
        return m_frameGraph.frameCounter;
    }

    frame_graph_build_context::frame_graph_build_context(frame_graph_impl& frameGraph,
        frame_graph_build_state& state,
        const frame_graph_build_args& args) : m_frameGraph{frameGraph}, m_state{state}, m_args{args}
    {
    }

    h32<compute_pass_instance> frame_graph_build_context::compute_pass(h32<oblo::compute_pass> pass,
        const compute_pipeline_initializer& initializer) const
    {
        const auto h = m_frameGraph.begin_pass_build(m_state, pass_kind::compute);
        auto& pm = m_args.rendererPlatform.passManager;
        m_frameGraph.passes[h.value].computePipeline = pm.get_or_create_pipeline(pass, initializer);

        return h32<compute_pass_instance>{h.value};
    }

    h32<render_pass_instance> frame_graph_build_context::render_pass(h32<oblo::render_pass> pass,
        const render_pipeline_initializer& initializer) const
    {
        const auto h = m_frameGraph.begin_pass_build(m_state, pass_kind::graphics);
        auto& pm = m_args.rendererPlatform.passManager;
        m_frameGraph.passes[h.value].renderPipeline = pm.get_or_create_pipeline(pass, initializer);

        return h32<render_pass_instance>{h.value};
    }

    h32<raytracing_pass_instance> frame_graph_build_context::raytracing_pass(h32<oblo::raytracing_pass> pass,
        const raytracing_pipeline_initializer& initializer) const
    {
        const auto h = m_frameGraph.begin_pass_build(m_state, pass_kind::raytracing);
        auto& pm = m_args.rendererPlatform.passManager;
        m_frameGraph.passes[h.value].raytracingPipeline = pm.get_or_create_pipeline(pass, initializer);

        return h32<raytracing_pass_instance>{h.value};
    }

    h32<transfer_pass_instance> frame_graph_build_context::transfer_pass() const
    {
        const auto h = m_frameGraph.begin_pass_build(m_state, pass_kind::transfer);
        return h32<transfer_pass_instance>{h.value};
    }

    h32<empty_pass_instance> frame_graph_build_context::empty_pass() const
    {
        const auto h = m_frameGraph.begin_pass_build(m_state, pass_kind::none);
        return h32<empty_pass_instance>{h.value};
    }

    const gpu_info& frame_graph_build_context::get_gpu_info() const
    {
        return m_frameGraph.gpuInfo;
    }

    bool frame_graph_build_context::is_recording_metrics() const
    {
        return m_frameGraph.is_recording_metrics();
    }

    void* frame_graph_build_context::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        return m_frameGraph.access_storage(handle);
    }

    bool frame_graph_build_context::has_event_impl(const type_id& type) const
    {
        return m_frameGraph.emptyEvents.contains(type);
    }

    void frame_graph_build_context::register_metrics_buffer(const type_id& type, pin::buffer b) const
    {
        m_frameGraph.add_metrics_download(type, b);
    }

    frame_graph_execute_context::frame_graph_execute_context(const frame_graph_impl& frameGraph,
        frame_graph_execution_state& executeCtx,
        const frame_graph_execute_args& args) : m_frameGraph{frameGraph}, m_state{executeCtx}, m_args{args}
    {
    }

    void frame_graph_execute_context::begin_pass(h32<frame_graph_pass> handle) const
    {
        m_frameGraph.begin_pass_execution(handle, m_state);
    }

    expected<> frame_graph_execute_context::begin_pass(h32<compute_pass_instance> handle) const
    {
        OBLO_ASSERT(handle);
        OBLO_ASSERT(m_frameGraph.passes[handle.value].kind == pass_kind::compute);

        const auto passHandle = h32<frame_graph_pass>{handle.value};
        m_frameGraph.begin_pass_execution(passHandle, m_state);

        auto& pm = m_args.rendererPlatform.passManager;

        const auto pipeline = m_frameGraph.passes[handle.value].computePipeline;
        const auto computeCtx = pm.begin_compute_pass(m_state.commandBuffer, pipeline);

        if (!computeCtx)
        {
            // Do we need to do anything?
            m_state.passKind = pass_kind::none;
            return "Compute pipeline context not found"_err;
        }

        m_state.passKind = pass_kind::compute;
        m_state.computeCtx = *computeCtx;
        m_state.basePipeline = pm.get_base_pipeline(computeCtx->internalPipeline);

        return no_error;
    }

    expected<> frame_graph_execute_context::begin_pass(h32<render_pass_instance> handle,
        const gpu::graphics_pass_descriptor& cfg) const
    {
        OBLO_ASSERT(handle);
        OBLO_ASSERT(m_frameGraph.passes[handle.value].kind == pass_kind::graphics);

        const auto passHandle = h32<frame_graph_pass>{handle.value};
        m_frameGraph.begin_pass_execution(passHandle, m_state);

        auto& pm = m_args.rendererPlatform.passManager;

        const auto pipeline = m_frameGraph.passes[handle.value].renderPipeline;
        const auto renderCtx = pm.begin_render_pass(m_state.commandBuffer, pipeline, cfg);

        if (!renderCtx)
        {
            // Do we need to do anything?
            m_state.passKind = pass_kind::none;
            return "Graphics pipeline context not found"_err;
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
        m_frameGraph.begin_pass_execution(passHandle, m_state);

        auto& pm = m_args.rendererPlatform.passManager;

        const auto pipeline = m_frameGraph.passes[handle.value].raytracingPipeline;
        const auto rtCtx = pm.begin_raytracing_pass(m_state.commandBuffer, pipeline);

        if (!rtCtx)
        {
            // Do we need to do anything?
            m_state.passKind = pass_kind::none;
            return "Raytracing pipeline context not found"_err;
        }

        m_state.passKind = pass_kind::raytracing;
        m_state.rtCtx = *rtCtx;
        m_state.basePipeline = pm.get_base_pipeline(rtCtx->internalPipeline);

        return no_error;
    }

    expected<> frame_graph_execute_context::begin_pass(h32<transfer_pass_instance> handle) const
    {
        OBLO_ASSERT(handle);
        OBLO_ASSERT(m_frameGraph.passes[handle.value].kind == pass_kind::transfer);

        const auto passHandle = h32<frame_graph_pass>{handle.value};
        m_frameGraph.begin_pass_execution(passHandle, m_state);

        m_state.passKind = pass_kind::transfer;

        return no_error;
    }

    expected<> frame_graph_execute_context::begin_pass(h32<empty_pass_instance> handle) const
    {
        OBLO_ASSERT(handle);
        OBLO_ASSERT(m_frameGraph.passes[handle.value].kind == pass_kind::none);

        const auto passHandle = h32<frame_graph_pass>{handle.value};
        m_frameGraph.begin_pass_execution(passHandle, m_state);

        m_state.passKind = pass_kind::none;

        return no_error;
    }

    void frame_graph_execute_context::end_pass() const
    {
        auto& pm = m_args.rendererPlatform.passManager;

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

        case pass_kind::transfer:
            break;

        case pass_kind::none:
            break;

        default:
            OBLO_ASSERT(false);
            break;
        }
    }

    void frame_graph_execute_context::bind_descriptor_sets(binding_tables_span bindingTables) const
    {
        binding_locator locator(m_frameGraph,
            m_args,
            m_state.imageStateTracker,
            bindingTables,
            *m_state.basePipeline,
            m_state.currentPass);

        switch (m_state.passKind)
        {
        case pass_kind::raytracing:
            m_args.rendererPlatform.passManager.bind_descriptor_sets(m_state.rtCtx, locator);
            break;

        case pass_kind::graphics:
            m_args.rendererPlatform.passManager.bind_descriptor_sets(m_state.renderCtx, locator);
            break;

        case pass_kind::compute:
            m_args.rendererPlatform.passManager.bind_descriptor_sets(m_state.computeCtx, locator);
            break;

        default:
            unreachable();
        }
    }

    void frame_graph_execute_context::bind_index_buffer(
        pin::buffer buffer, u64 bufferOffset, gpu::mesh_index_type indexType) const
    {
        const frame_graph_buffer_impl& b = access_storage_resource(m_frameGraph, buffer);
        m_state.gpu->cmd_bind_index_buffer(m_state.commandBuffer, b.handle, b.offset + bufferOffset, indexType);
    }

    gpu::buffer_range frame_graph_execute_context::access(pin::buffer handle) const
    {
        const frame_graph_buffer_impl& b = access_storage_resource(m_frameGraph, handle);
        return {b.handle, b.offset, b.size};
    }

    h32<gpu::image> frame_graph_execute_context::access(pin::texture handle) const
    {
        const frame_graph_texture_impl& t = access_storage_resource(m_frameGraph, handle);
        return t.handle;
    }

    pin::acceleration_structure frame_graph_execute_context::get_global_tlas() const
    {
        return g_globalTLAS;
    }

    bool frame_graph_execute_context::has_source(pin::buffer buffer) const
    {
        auto* const owner = m_frameGraph.get_owner_node(buffer);
        return m_frameGraph.currentNode != owner;
    }

    bool frame_graph_execute_context::has_source(pin::texture texture) const
    {
        auto* const owner = m_frameGraph.get_owner_node(texture);
        return m_frameGraph.currentNode != owner;
    }

    u32 frame_graph_execute_context::get_frames_alive_count(pin::texture texture) const
    {
        const auto h = m_frameGraph.find_pool_index(texture);
        OBLO_ASSERT(h);
        return m_frameGraph.resourcePool.get_frames_alive_count(h);
    }

    u32 frame_graph_execute_context::get_frames_alive_count(pin::buffer buffer) const
    {
        const auto h = m_frameGraph.find_pool_index(buffer);
        OBLO_ASSERT(h);
        return m_frameGraph.resourcePool.get_frames_alive_count(h);
    }

    u32 frame_graph_execute_context::get_current_frames_count() const
    {
        return m_frameGraph.frameCounter;
    }

    void frame_graph_execute_context::upload(pin::buffer h, std::span<const byte> data, u64 bufferOffset) const
    {
        OBLO_ASSERT(m_state.currentPass && m_frameGraph.passes[m_state.currentPass.value].kind == pass_kind::transfer);

        auto& stagingBuffer = m_args.stagingBuffer;
        const auto stagedData = stagingBuffer.stage(data);

        if (!stagedData)
        {
            OBLO_ASSERT(stagedData);
            return;
        }

        const auto b = access_storage_resource(m_frameGraph, h);
        stagingBuffer.upload(m_state.commandBuffer, *stagedData, b.handle, b.offset + bufferOffset);
    }

    void frame_graph_execute_context::upload(pin::buffer h, const staging_buffer_span& data, u64 bufferOffset) const
    {
        auto& stagingBuffer = m_args.stagingBuffer;
        const frame_graph_buffer_impl& b = access_storage_resource(m_frameGraph, h);

        // NOTE: This is also not thread safe, it changes some internal state in the staging buffer
        stagingBuffer.upload(m_state.commandBuffer, data, b.handle, b.offset + bufferOffset);
    }

    void frame_graph_execute_context::upload(pin::texture h, const staging_buffer_span& data) const
    {
        auto& stagingBuffer = m_args.stagingBuffer;
        const frame_graph_texture_impl& t = access_storage_resource(m_frameGraph, h);

        gpu::buffer_image_copy_descriptor fullCopy[2]{};

        u32 copies = 0;

        for (const auto& segment : data.segments)
        {
            if (segment.begin != segment.end)
            {
                constexpr u32 levelIndex = 0;

                fullCopy[copies] = {
                    .bufferOffset = segment.begin,
                    .imageSubresource =
                        {
                            .mipLevel = levelIndex,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                        },
                    .imageOffset = {}, // Zero because we do a full copy
                    .imageExtent = {t.descriptor.width,
                        t.descriptor.height,
                        t.descriptor.depth}, // Full extent because we assume level index 0 too
                };

                ++copies;
            }
        }

        if (copies > 0)
        {
            // NOTE: This is also not thread safe, it changes some internal state in the staging buffer
            stagingBuffer.upload(m_state.commandBuffer, t.handle, {fullCopy, copies});
        }
    }

    async_download frame_graph_execute_context::download(pin::buffer h) const
    {
        OBLO_ASSERT(h);
        OBLO_ASSERT(m_state.currentPass);
        OBLO_ASSERT(m_state.passKind == pass_kind::transfer);

        auto& pass = m_frameGraph.passes[m_state.currentPass.value];

        for (u32 i = pass.bufferDownloadBegin; i < pass.bufferDownloadEnd; ++i)
        {
            auto& download = m_frameGraph.bufferDownloads[i];

            if (as_storage_handle(h) == download.pinStorage)
            {
                auto& pendingDownload = m_frameGraph.pendingDownloads[download.pendingDownloadId];

                const frame_graph_buffer_impl& b = access_storage_resource(m_frameGraph, h);
                m_frameGraph.downloadStaging.download(m_state.commandBuffer,
                    b.handle,
                    b.offset,
                    pendingDownload.stagedSpan);

                return async_download{pendingDownload.promise};
            }
        }

        OBLO_ASSERT(false, "The download was not declared in the build process");
        return async_download{};
    }

    h64<gpu::device_address> frame_graph_execute_context::get_device_address(pin::buffer buffer) const
    {
        const frame_graph_buffer_impl& b = access_storage_resource(m_frameGraph, buffer);
        return m_state.gpu->get_device_address({b.handle, b.offset, b.size});
    }

    const gpu_info& frame_graph_execute_context::get_gpu_info() const
    {
        return m_frameGraph.gpuInfo;
    }

    void frame_graph_execute_context::set_viewport(u32 w, u32 h, f32 minDepth, f32 maxDepth) const
    {
        const gpu::rectangle viewport{
            .width = w,
            .height = h,
        };

        m_state.gpu->cmd_set_viewport(m_state.commandBuffer, 0, {&viewport, 1}, minDepth, maxDepth);
    }

    void frame_graph_execute_context::set_scissor(i32 x, i32 y, u32 w, u32 h) const
    {
        const gpu::rectangle scissor{
            .x = x,
            .y = y,
            .width = w,
            .height = h,
        };

        m_state.gpu->cmd_set_scissor(m_state.commandBuffer, 0, {&scissor, 1});
    }

    void frame_graph_execute_context::push_constants(
        flags<gpu::shader_stage> stages, u32 offset, std::span<const byte> bytes) const
    {
        auto& pm = m_args.rendererPlatform.passManager;

        switch (m_state.passKind)
        {
        case pass_kind::compute:
            pm.push_constants(m_state.computeCtx, stages, offset, bytes);
            break;

        case pass_kind::graphics:
            pm.push_constants(m_state.renderCtx, stages, offset, bytes);
            break;

        case pass_kind::raytracing:
            pm.push_constants(m_state.rtCtx, stages, offset, bytes);
            break;

        case pass_kind::transfer:
            break;

        case pass_kind::none:
            break;

        default:
            OBLO_ASSERT(false);
            break;
        }
    }

    void frame_graph_execute_context::dispatch_compute(u32 groupsX, u32 groupsY, u32 groupsZ) const
    {
        OBLO_ASSERT(m_state.passKind == pass_kind::compute);
        m_state.gpu->cmd_dispatch_compute(m_state.commandBuffer, groupsX, groupsY, groupsZ);
    }

    void frame_graph_execute_context::trace_rays(u32 x, u32 y, u32 z) const
    {
        OBLO_ASSERT(m_state.passKind == pass_kind::raytracing);
        auto& pm = m_args.rendererPlatform.passManager;
        pm.trace_rays(m_state.rtCtx, x, y, z);
    }

    void frame_graph_execute_context::draw_indexed(
        u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const
    {
        m_state.gpu->cmd_draw_indexed(m_state.commandBuffer,
            indexCount,
            instanceCount,
            firstIndex,
            vertexOffset,
            firstInstance);
    }

    void frame_graph_execute_context::draw_mesh_tasks_indirect_count(pin::buffer drawCallBuffer,
        u32 drawCallBufferOffset,
        pin::buffer drawCallCountBuffer,
        u32 drawCallCountBufferOffset,
        u32 maxDrawCount) const
    {
        const frame_graph_buffer_impl& drawCallBuf = access_storage_resource(m_frameGraph, drawCallBuffer);
        const frame_graph_buffer_impl& drawCountBuf = access_storage_resource(m_frameGraph, drawCallCountBuffer);

        m_state.gpu->cmd_draw_mesh_tasks_indirect_count(m_state.commandBuffer,
            drawCallBuf.handle,
            drawCallBufferOffset + drawCallBuf.offset,
            drawCountBuf.handle,
            drawCallCountBufferOffset + drawCountBuf.offset,
            maxDrawCount);
    }

    void frame_graph_execute_context::blit_color(pin::texture srcTexture, pin::texture dstTexture) const
    {
        const frame_graph_texture_impl& src = access_storage_resource(m_frameGraph, srcTexture);
        const frame_graph_texture_impl& dst = access_storage_resource(m_frameGraph, dstTexture);

        m_state.gpu->cmd_blit(m_state.commandBuffer, src.handle, dst.handle, gpu::sampler_filter::linear);
    }

    vec2u frame_graph_execute_context::get_resolution(pin::texture h) const
    {
        const frame_graph_texture_impl& t = access_storage_resource(m_frameGraph, h);
        return {t.descriptor.width, t.descriptor.height};
    }

    bool frame_graph_execute_context::is_recording_metrics() const
    {
        return m_frameGraph.is_recording_metrics();
    }

    void* frame_graph_execute_context::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        return m_frameGraph.access_storage(handle);
    }

    bool frame_graph_execute_context::has_event_impl(const type_id& type) const
    {
        return m_frameGraph.emptyEvents.contains(type);
    }

    frame_graph_init_context::frame_graph_init_context(frame_graph_impl& frameGraph,
        const frame_graph_build_args& args) : m_frameGraph{frameGraph}, m_args{args}
    {
    }

    h32<compute_pass> frame_graph_init_context::register_compute_pass(const compute_pass_initializer& initializer) const
    {
        return m_args.rendererPlatform.passManager.register_compute_pass(initializer);
    }

    h32<render_pass> frame_graph_init_context::register_render_pass(const render_pass_initializer& initializer) const
    {
        return m_args.rendererPlatform.passManager.register_render_pass(initializer);
    }

    h32<raytracing_pass> frame_graph_init_context::register_raytracing_pass(
        const raytracing_pass_initializer& initializer) const
    {
        return m_args.rendererPlatform.passManager.register_raytracing_pass(initializer);
    }

    const gpu_info& frame_graph_init_context::get_gpu_info() const
    {
        return m_frameGraph.gpuInfo;
    }
}