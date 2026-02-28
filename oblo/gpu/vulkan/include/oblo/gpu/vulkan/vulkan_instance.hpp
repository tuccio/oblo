#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/vulkan/descriptor_set_pool.hpp>
#include <oblo/gpu/vulkan/error.hpp>
#include <oblo/gpu/vulkan/gpu_allocator.hpp>
#include <oblo/gpu/vulkan/utility/debug_utils.hpp>
#include <oblo/gpu/vulkan/utility/loaded_functions.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
{
    class vulkan_instance final : public gpu_instance
    {
    public:
        vulkan_instance();
        vulkan_instance(const vulkan_instance&) = delete;
        vulkan_instance(vulkan_instance&&) noexcept = delete;
        ~vulkan_instance();

        vulkan_instance& operator=(const vulkan_instance&) = delete;
        vulkan_instance& operator=(vulkan_instance&&) noexcept = delete;

        result<> init(const instance_descriptor& descriptor) override;
        void shutdown() override;

        result<hptr<surface>> create_surface(hptr<native_window> nativeWindow) override;
        void destroy(hptr<surface> surface) override;

        result<> finalize_init(const device_descriptor& deviceDescriptor, hptr<surface> presentSurface) override;

        h32<queue> get_universal_queue() override;

        device_info get_device_info() override;

        bool is_profiler_attached() const override;

        result<h32<swapchain>> create_swapchain(const swapchain_descriptor& descriptor) override;
        void destroy(h32<swapchain> handle) override;

        result<h32<fence>> create_fence(const fence_descriptor& descriptor) override;
        void destroy(h32<fence> handle) override;

        result<> wait_for_fences(const std::span<const h32<fence>> fences) override;
        result<> reset_fences(const std::span<const h32<fence>> fences) override;

        result<h32<semaphore>> create_semaphore(const semaphore_descriptor& descriptor) override;
        void destroy(h32<semaphore> handle) override;
        result<u64> read_timeline_semaphore(h32<semaphore> handle) override;

        result<h32<image>> acquire_swapchain_image(h32<swapchain> handle, h32<semaphore> waitSemaphore) override;

        result<h32<bind_group_layout>> create_bind_group_layout(
            const bind_group_layout_descriptor& descriptor) override;

        void destroy(h32<bind_group_layout> handle) override;

        result<hptr<bind_group>> acquire_transient_bind_group(h32<bind_group_layout> handle,
            std::span<const bind_group_data> data) override;

        u32 get_max_bindless_images() const override;

        result<> set_bindless_images(std::span<const bindless_image_descriptor> images, u32 first) override;

        result<hptr<bind_group>> acquire_transient_bindless_images_bind_group(
            h32<bind_group_layout> handle, u32 binding, u32 count) override;

        result<h32<command_buffer_pool>> create_command_buffer_pool(
            const command_buffer_pool_descriptor& descriptor) override;

        void destroy(h32<command_buffer_pool> commandBufferPool) override;

        result<> reset_command_buffer_pool(h32<command_buffer_pool> commandBufferPool) override;

        result<> fetch_command_buffers(h32<command_buffer_pool> pool,
            std::span<hptr<command_buffer>> commandBuffers) override;

        result<> begin_command_buffer(hptr<command_buffer> commandBuffer) override;
        result<> end_command_buffer(hptr<command_buffer> commandBuffer) override;

        result<h32<buffer>> create_buffer(const buffer_descriptor& descriptor) override;
        void destroy(h32<buffer> bufferHandle) override;

        h64<device_address> get_device_address(h32<buffer> bufferHandle) override;
        h64<device_address> get_device_address(buffer_range bufferWithOffset) override;

        result<h32<acceleration_structure>> create_acceleration_structure(
            const acceleration_structure_descriptor& descriptor) override;

        void destroy(h32<acceleration_structure> handle) override;

        result<h32<image>> create_image(const image_descriptor& descriptor) override;
        void destroy(h32<image> imageHandle) override;

        image_descriptor get_image_descriptor(h32<image> imageHandle) override;

        result<h32<image_pool>> create_image_pool(std::span<const image_descriptor> descriptors,
            std::span<h32<image>> images) override;

        void destroy(h32<image_pool> imagePoolHandle) override;

        result<h32<shader_module>> create_shader_module(const shader_module_descriptor& descriptor) override;
        void destroy(h32<shader_module> handle) override;

        result<h32<graphics_pipeline>> create_graphics_pipeline(
            const graphics_pipeline_descriptor& descriptor) override;
        void destroy(h32<graphics_pipeline> handle) override;

        result<h32<compute_pipeline>> create_compute_pipeline(const compute_pipeline_descriptor& descriptor) override;
        void destroy(h32<compute_pipeline> handle) override;

        result<h32<raytracing_pipeline>> create_raytracing_pipeline(
            const raytracing_pipeline_descriptor& descriptor) override;
        void destroy(h32<raytracing_pipeline> handle) override;

        result<h32<sampler>> create_sampler(const sampler_descriptor& descriptor) override;
        void destroy(h32<sampler> handle) override;

        result<> begin_graphics_pass(hptr<command_buffer> cmdBuffer,
            h32<graphics_pipeline> pipeline,
            const graphics_pass_descriptor& descriptor) override;

        void end_graphics_pass(hptr<command_buffer> cmdBuffer) override;

        result<> begin_compute_pass(hptr<command_buffer> cmdBuffer, h32<compute_pipeline> pipeline) override;

        void end_compute_pass(hptr<command_buffer> cmdBuffer) override;

        result<> begin_raytracing_pass(hptr<command_buffer> cmdBuffer, h32<raytracing_pipeline> pipeline) override;

        void end_raytracing_pass(hptr<command_buffer> cmdBuffer) override;

        result<> begin_submit_tracking() override;
        result<> submit(h32<queue> handle, const queue_submit_descriptor& descriptor) override;

        result<> present(const present_descriptor& descriptor) override;

        result<> wait_idle() override;

        // Memory mapping

        result<void*> memory_map(h32<buffer> buffer) override;

        result<> memory_unmap(h32<buffer> buffer) override;

        result<> memory_invalidate(std::span<const h32<buffer>> buffers) override;

        // Transfer commands

        void cmd_copy_buffer(hptr<command_buffer> cmd,
            h32<buffer> src,
            h32<buffer> dst,
            std::span<const buffer_copy_descriptor> copies) override;

        void cmd_copy_buffer_to_image(hptr<command_buffer> cmd,
            h32<buffer> src,
            h32<image> dst,
            std::span<const buffer_image_copy_descriptor> copies) override;

        void cmd_blit(hptr<command_buffer> cmd, h32<image> src, h32<image> dst, gpu::sampler_filter filter) override;

        // Barriers and synchronization

        void cmd_apply_barriers(hptr<command_buffer> cmd, const memory_barrier_descriptor& descriptor) override;

        // Draw

        void cmd_draw(
            hptr<command_buffer> cmd, u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override;

        void cmd_draw_indexed(hptr<command_buffer> cmd,
            u32 indexCount,
            u32 instanceCount,
            u32 firstIndex,
            u32 vertexOffset,
            u32 firstInstance) override;

        void cmd_draw_mesh_tasks_indirect_count(hptr<command_buffer> cmd,
            h32<buffer> drawBuffer,
            u64 drawOffset,
            h32<buffer> countBuffer,
            u64 countOffset,
            u32 maxDrawCount) override;

        void cmd_dispatch_compute(hptr<command_buffer> cmd, u32 groupX, u32 groupY, u32 groupZ) override;

        void cmd_trace_rays(hptr<command_buffer> cmd,
            h32<raytracing_pipeline> currentPipeline,
            u32 width,
            u32 height,
            u32 depth) override;

        void cmd_set_viewport(hptr<command_buffer> cmd,
            u32 firstScissor,
            std::span<const rectangle> viewports,
            f32 minDepth,
            f32 maxDepth) override;

        void cmd_set_scissor(hptr<command_buffer> cmd, u32 firstScissor, std::span<const rectangle> scissors) override;

        void cmd_bind_index_buffer(
            hptr<command_buffer> cmd, h32<buffer> buffer, u64 offset, gpu::mesh_index_type format) override;

        void cmd_bind_groups(hptr<command_buffer> cmd,
            h32<graphics_pipeline> pipeline,
            u32 firstGroup,
            std::span<const hptr<bind_group>> bindGroups) override;

        void cmd_bind_groups(hptr<command_buffer> cmd,
            h32<compute_pipeline> pipeline,
            u32 firstGroup,
            std::span<const hptr<bind_group>> bindGroups) override;

        void cmd_bind_groups(hptr<command_buffer> cmd,
            h32<raytracing_pipeline> pipeline,
            u32 firstGroup,
            std::span<const hptr<bind_group>> bindGroups) override;

        void cmd_push_constants(hptr<command_buffer> cmd,
            h32<graphics_pipeline> pipeline,
            flags<shader_stage> shaderStages,
            u32 offset,
            std::span<const byte> data) override;

        void cmd_push_constants(hptr<command_buffer> cmd,
            h32<compute_pipeline> pipeline,
            flags<shader_stage> shaderStages,
            u32 offset,
            std::span<const byte> data) override;

        void cmd_push_constants(hptr<command_buffer> cmd,
            h32<raytracing_pipeline> pipeline,
            flags<shader_stage> shaderStages,
            u32 offset,
            std::span<const byte> data) override;

        // Debugging and profiling

        void cmd_label_begin(hptr<command_buffer> cmd, cstring_view label) override;
        void cmd_label_end(hptr<command_buffer> cmd) override;

        result<hptr<profiling_context>> cmd_profile_begin(hptr<command_buffer> cmd, cstring_view label) override;
        void cmd_profile_end(hptr<command_buffer> cmd, hptr<profiling_context> context) override;

        void cmd_profile_collect_metrics(hptr<command_buffer> cmd) override;

        // Vulkan specific
        VkInstance get_instance() const;
        VkPhysicalDevice get_physical_device() const;
        VkDevice get_device() const;
        gpu_allocator& get_allocator();

        VkAccelerationStructureKHR unwrap_acceleration_structure(h32<acceleration_structure> handle) const;
        VkBuffer unwrap_buffer(h32<buffer> handle) const;
        VkCommandBuffer unwrap_command_buffer(hptr<command_buffer> handle) const;
        VkImage unwrap_image(h32<image> handle) const;
        VkImageView unwrap_image_view(h32<image> handle) const;
        VkQueue unwrap_queue(h32<queue> handle) const;
        VkShaderModule unwrap_shader_module(h32<shader_module> handle) const;

        debug_utils::object get_object_labeler() const;

        const loaded_functions& get_loaded_functions() const;

        // Very hacky, it's just here as a temporary solution to get things running without support for acceleration
        // structures
        h32<gpu::acceleration_structure> register_acceleration_structure(
            VkAccelerationStructureKHR accelerationStructure);

    private:
        struct acceleration_structure_impl;
        struct bind_group_layout_impl;
        struct buffer_impl;
        struct command_buffer_pool_impl;
        struct compute_pipeline_impl;
        struct image_impl;
        struct image_pool_impl;
        struct queue_impl;
        struct graphics_pipeline_impl;
        struct raytracing_pipeline_impl;
        struct sampler_impl;
        struct shader_module_impl;
        struct swapchain_impl;

        struct profiling_impl;

    private:
        h32<image> register_image(
            VkImage image, VkImageView view, VmaAllocation allocation, const image_descriptor& descriptor);

        void unregister_image(h32<image> image);

        const queue_impl& get_queue(h32<queue> queue) const;

        template <typename T>
        void label_vulkan_object(T obj, const debug_label& label);

        result<> create_pipeline_layout(std::span<const push_constant_range> pushConstants,
            std::span<const h32<bind_group_layout>> bindGroupLayouts,
            VkPipelineLayout* pipelineLayout,
            const debug_label& label);

        void initialize_descriptor_set(VkDescriptorSet descriptorSet, std::span<const bind_group_data> data);

        VkSampler get_or_create_dummy_sampler();

    private:
        VkInstance m_instance{};
        VkPhysicalDevice m_physicalDevice{};
        VkDevice m_device{};
        gpu_allocator m_allocator;
        dynamic_array<queue_impl> m_queues;

        unique_ptr<profiling_impl> m_profiling;

        h32<sampler> m_dummySampler{};

        // Lazy approach with all these maps, we don't iterate over them so another solution would be preferrable
        h32_flat_pool_dense_map<acceleration_structure, acceleration_structure_impl> m_accelerationStructures;
        h32_flat_pool_dense_map<swapchain, swapchain_impl> m_swapchains;
        h32_flat_pool_dense_map<buffer, buffer_impl> m_buffers;
        h32_flat_pool_dense_map<image, image_impl> m_images;
        h32_flat_pool_dense_map<image_pool, image_pool_impl> m_imagePools;
        h32_flat_pool_dense_map<semaphore, VkSemaphore> m_semaphores;
        h32_flat_pool_dense_map<command_buffer_pool, command_buffer_pool_impl> m_commandBufferPools;
        h32_flat_pool_dense_map<fence, VkFence> m_fences;
        h32_flat_pool_dense_map<shader_module, shader_module_impl> m_shaderModules;
        h32_flat_pool_dense_map<sampler, sampler_impl> m_samplers;
        h32_flat_pool_dense_map<bind_group_layout, bind_group_layout_impl> m_bindGroupLayouts;
        h32_flat_pool_dense_map<graphics_pipeline, graphics_pipeline_impl> m_renderPipelines;
        h32_flat_pool_dense_map<compute_pipeline, compute_pipeline_impl> m_computePipelines;
        h32_flat_pool_dense_map<raytracing_pipeline, raytracing_pipeline_impl> m_raytracingPipelines;

        dynamic_array<VkDescriptorImageInfo> m_bindlessImages;

        descriptor_set_pool m_perFrameSetPool;

        VkPhysicalDeviceProperties2 m_physicalDeviceProperties{};
        VkPhysicalDeviceAccelerationStructurePropertiesKHR m_accelerationStructureProperties{};
        VkPhysicalDeviceSubgroupProperties m_subgroupProperties{};
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_raytracingProperties{};

        debug_utils::label m_cmdLabeler{};
        debug_utils::object m_objLabeler{};
        loaded_functions m_loadedFunctions{};
    };
}