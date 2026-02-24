#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/error.hpp>
#include <oblo/gpu/forward.hpp>

#include <span>

namespace oblo::gpu
{
    class gpu_instance
    {
    public:
        gpu_instance();

        gpu_instance(const gpu_instance&) = delete;
        gpu_instance(gpu_instance&&) = delete;

        virtual ~gpu_instance();

        gpu_instance& operator=(const gpu_instance&) = delete;
        gpu_instance& operator=(gpu_instance&&) = delete;

        virtual result<> init(const instance_descriptor& descriptor) = 0;
        virtual void shutdown() = 0;

        virtual result<hptr<surface>> create_surface(hptr<native_window> nativeWindow) = 0;
        virtual void destroy(hptr<surface> surface) = 0;

        virtual result<> finalize_init(const device_descriptor& deviceDescriptor, hptr<surface> presentSurface) = 0;

        virtual device_info get_device_info() = 0;

        virtual h32<queue> get_universal_queue() = 0;

        virtual result<h32<swapchain>> create_swapchain(const swapchain_descriptor& descriptor) = 0;
        virtual void destroy(h32<swapchain> handle) = 0;

        virtual result<h32<fence>> create_fence(const fence_descriptor& descriptor) = 0;
        virtual void destroy(h32<fence> handle) = 0;

        virtual result<> wait_for_fences(const std::span<const h32<fence>> fences) = 0;
        virtual result<> reset_fences(const std::span<const h32<fence>> fences) = 0;

        virtual result<h32<semaphore>> create_semaphore(const semaphore_descriptor& descriptor) = 0;
        virtual void destroy(h32<semaphore> handle) = 0;
        virtual result<u64> read_timeline_semaphore(h32<semaphore> handle) = 0;

        virtual result<h32<image>> acquire_swapchain_image(h32<swapchain> handle, h32<semaphore> waitSemaphore) = 0;

        virtual result<h32<bind_group_layout>> create_bind_group_layout(
            const bind_group_layout_descriptor& descriptor) = 0;

        virtual void destroy(h32<bind_group_layout> handle) = 0;

        virtual result<hptr<bind_group>> acquire_transient_bind_group(h32<bind_group_layout> handle,
            std::span<const bind_group_data> data) = 0;

        virtual result<hptr<bind_group>> acquire_transient_variable_bind_group(
            h32<bind_group_layout> handle, std::span<const bind_group_data> data, u32 count) = 0;

        virtual result<h32<command_buffer_pool>> create_command_buffer_pool(
            const command_buffer_pool_descriptor& descriptor) = 0;

        virtual result<> fetch_command_buffers(h32<command_buffer_pool> pool,
            std::span<hptr<command_buffer>> commandBuffers) = 0;

        virtual void destroy(h32<command_buffer_pool> commandBufferPool) = 0;

        virtual result<> reset_command_buffer_pool(h32<command_buffer_pool> commandBufferPool) = 0;

        virtual result<> begin_command_buffer(hptr<command_buffer> commandBuffer) = 0;
        virtual result<> end_command_buffer(hptr<command_buffer> commandBuffer) = 0;

        virtual result<h32<buffer>> create_buffer(const buffer_descriptor& descriptor) = 0;
        virtual void destroy(h32<buffer> bufferHandle) = 0;

        virtual h64<device_address> get_device_address(h32<buffer> bufferHandle) = 0;
        virtual h64<device_address> get_device_address(buffer_range bufferWithOffset) = 0;

        virtual result<h32<acceleration_structure>> create_acceleration_structure(
            const acceleration_structure_descriptor& descriptor) = 0;
        virtual void destroy(h32<acceleration_structure> handle) = 0;

        virtual result<h32<image>> create_image(const image_descriptor& descriptor) = 0;
        virtual void destroy(h32<image> imageHandle) = 0;

        virtual image_descriptor get_image_descriptor(h32<image> imageHandle) = 0;

        virtual result<h32<image_pool>> create_image_pool(std::span<const image_descriptor> descriptors,
            std::span<h32<image>> images) = 0;

        virtual void destroy(h32<image_pool> imagePoolHandle) = 0;

        virtual result<h32<shader_module>> create_shader_module(const shader_module_descriptor& descriptor) = 0;
        virtual void destroy(h32<shader_module> handle) = 0;

        virtual result<h32<graphics_pipeline>> create_graphics_pipeline(
            const graphics_pipeline_descriptor& descriptor) = 0;
        virtual void destroy(h32<graphics_pipeline> handle) = 0;

        virtual result<h32<compute_pipeline>> create_compute_pipeline(
            const compute_pipeline_descriptor& descriptor) = 0;
        virtual void destroy(h32<compute_pipeline> handle) = 0;

        virtual result<h32<raytracing_pipeline>> create_raytracing_pipeline(
            const raytracing_pipeline_descriptor& descriptor) = 0;
        virtual void destroy(h32<raytracing_pipeline> handle) = 0;

        virtual result<h32<sampler>> create_sampler(const sampler_descriptor& descriptor) = 0;
        virtual void destroy(h32<sampler> handle) = 0;

        virtual result<hptr<graphics_pass>> begin_graphics_pass(hptr<command_buffer> cmdBuffer,
            h32<graphics_pipeline> pipeline,
            const graphics_pass_descriptor& descriptor) = 0;

        virtual void end_graphics_pass(hptr<command_buffer> cmdBuffer, hptr<graphics_pass> renderPass) = 0;

        virtual result<hptr<compute_pass>> begin_compute_pass(hptr<command_buffer> cmdBuffer,
            h32<compute_pipeline> pipeline) = 0;

        virtual void end_compute_pass(hptr<command_buffer> cmdBuffer, hptr<compute_pass> renderPass) = 0;

        virtual result<hptr<raytracing_pass>> begin_raytracing_pass(hptr<command_buffer> cmdBuffer,
            h32<raytracing_pipeline> pipeline) = 0;

        virtual void end_raytracing_pass(hptr<command_buffer> cmdBuffer, hptr<raytracing_pass> renderPass) = 0;

        /// @brief Necessary to call for tracking the main queue and synchronizing with the GPU when necessary.
        /// This function might release resources that are not used by the GPU anymore.
        /// The end of the tracking happens upon submission on the main queue.
        virtual result<> begin_submit_tracking() = 0;
        virtual result<> submit(h32<queue> handle, const queue_submit_descriptor& descriptor) = 0;

        virtual result<> present(const present_descriptor& descriptor) = 0;

        virtual result<> wait_idle() = 0;

        u64 get_submit_index() const;
        u64 get_last_finished_submit() const;
        bool is_submit_done(u64 submitIndex) const;

        result<> wait_for_submit_completion(u64 submitIndex);

        void destroy_deferred(h32<acceleration_structure> h, u64 submitIndex);
        void destroy_deferred(h32<bind_group_layout> h, u64 submitIndex);
        void destroy_deferred(h32<buffer> h, u64 submitIndex);
        void destroy_deferred(h32<command_buffer_pool> h, u64 submitIndex);
        void destroy_deferred(h32<compute_pipeline> h, u64 submitIndex);
        void destroy_deferred(h32<fence> h, u64 submitIndex);
        void destroy_deferred(h32<graphics_pipeline> h, u64 submitIndex);
        void destroy_deferred(h32<image> h, u64 submitIndex);
        void destroy_deferred(h32<image_pool> h, u64 submitIndex);
        void destroy_deferred(h32<raytracing_pipeline> h, u64 submitIndex);
        void destroy_deferred(h32<sampler> h, u64 submitIndex);
        void destroy_deferred(h32<semaphore> h, u64 submitIndex);

        // Memory mapping

        virtual result<void*> memory_map(h32<buffer> buffer) = 0;

        virtual result<> memory_unmap(h32<buffer> buffer) = 0;

        virtual result<> memory_invalidate(std::span<const h32<buffer>> buffers) = 0;

        // Transfer commands

        virtual void cmd_copy_buffer(hptr<command_buffer> cmd,
            h32<buffer> src,
            h32<buffer> dst,
            std::span<const buffer_copy_descriptor> copies) = 0;

        virtual void cmd_copy_buffer_to_image(hptr<command_buffer> cmd,
            h32<buffer> src,
            h32<image> dst,
            std::span<const buffer_image_copy_descriptor> copies) = 0;

        // Barriers and synchronization

        virtual void cmd_apply_barriers(hptr<command_buffer> cmd, const memory_barrier_descriptor& descriptor) = 0;

        // Draw

        virtual void cmd_draw(
            hptr<command_buffer> cmd, u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) = 0;

        virtual void cmd_dispatch_compute(hptr<command_buffer> cmd, u32 groupX, u32 groupY, u32 groupZ) = 0;

        virtual void cmd_trace_rays(
            hptr<command_buffer> cmd, hptr<raytracing_pass> currentPass, u32 width, u32 height, u32 depth) = 0;

        virtual void cmd_set_viewport(hptr<command_buffer> cmd,
            u32 firstViewport,
            std::span<const rectangle> viewports,
            f32 minDepth,
            f32 maxDepth) = 0;

        virtual void cmd_set_scissor(
            hptr<command_buffer> cmd, u32 firstScissor, std::span<const rectangle> scissors) = 0;

        virtual void cmd_bind_groups(hptr<command_buffer> cmd,
            h32<graphics_pipeline> pipeline,
            u32 firstGroup,
            std::span<const hptr<bind_group>> bindGroups) = 0;

        virtual void cmd_bind_groups(hptr<command_buffer> cmd,
            h32<compute_pipeline> pipeline,
            u32 firstGroup,
            std::span<const hptr<bind_group>> bindGroups) = 0;

        virtual void cmd_bind_groups(hptr<command_buffer> cmd,
            h32<raytracing_pipeline> pipeline,
            u32 firstGroup,
            std::span<const hptr<bind_group>> bindGroups) = 0;

        virtual void cmd_push_constants(hptr<command_buffer> cmd,
            h32<graphics_pipeline> pipeline,
            flags<shader_stage> shaderStages,
            u32 offset,
            std::span<const byte> data) = 0;

        virtual void cmd_push_constants(hptr<command_buffer> cmd,
            h32<compute_pipeline> pipeline,
            flags<shader_stage> shaderStages,
            u32 offset,
            std::span<const byte> data) = 0;

        virtual void cmd_push_constants(hptr<command_buffer> cmd,
            h32<raytracing_pipeline> pipeline,
            flags<shader_stage> shaderStages,
            u32 offset,
            std::span<const byte> data) = 0;

        // Debugging and profiling

        virtual void cmd_label_begin(hptr<command_buffer> cmd, const char* label) = 0;
        virtual void cmd_label_end(hptr<command_buffer> cmd) = 0;

    protected:
        struct submit_info;
        struct disposable_object;

        result<> init_tracked_queue_context();
        void shutdown_tracked_queue_context();

        void destroy_tracked_queue_resources_until(u64 lastCompletedSubmit);

        result<> begin_tracked_queue_submit();
        void end_tracked_queue_submit();

        h32<fence> get_tracked_queue_fence();

        template <typename T>
        void destroy_deferred_impl(h32<T> handle, u64 submitIndex);

    protected:
        // We want the submit index to start from more than 0, which is the starting value of the semaphore
        u64 m_submitIndex{1};
        u64 m_lastFinishedSubmit{};

        h32<semaphore> m_timelineSemaphore{};
        h32<queue> m_queue{};

        dynamic_array<submit_info> m_submitInfo;
        deque<disposable_object> m_objectsToDispose;
    };

    inline u64 gpu_instance::get_submit_index() const
    {
        return m_submitIndex;
    }

    inline u64 gpu_instance::get_last_finished_submit() const
    {
        return m_lastFinishedSubmit;
    }

    inline bool gpu_instance::is_submit_done(u64 submitIndex) const
    {
        return m_lastFinishedSubmit >= submitIndex;
    }

    /// @brief To be used to offset device address, to keep track where we make the assumptions on how to offset
    /// addresses.
    inline h64<device_address> offset_device_address(h64<device_address> address, u64 offset)
    {
        return {address.value + offset};
    }
}