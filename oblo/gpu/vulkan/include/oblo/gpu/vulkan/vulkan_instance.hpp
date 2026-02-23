#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/vulkan/descriptor_set_pool.hpp>
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
        void destroy_surface(hptr<surface> surface) override;

        result<> finalize_init(const device_descriptor& deviceDescriptor, hptr<surface> presentSurface) override;

        h32<queue> get_universal_queue() override;

        device_info get_device_info() override;

        result<h32<swapchain>> create_swapchain(const swapchain_descriptor& descriptor) override;
        void destroy_swapchain(h32<swapchain> handle) override;

        result<h32<fence>> create_fence(const fence_descriptor& descriptor) override;
        void destroy_fence(h32<fence> handle) override;

        result<> wait_for_fences(const std::span<const h32<fence>> fences) override;
        result<> reset_fences(const std::span<const h32<fence>> fences) override;

        result<h32<semaphore>> create_semaphore(const semaphore_descriptor& descriptor) override;
        void destroy_semaphore(h32<semaphore> handle) override;
        result<u64> read_timeline_semaphore(h32<semaphore> handle) override;

        result<h32<image>> acquire_swapchain_image(h32<swapchain> handle, h32<semaphore> waitSemaphore) override;

        result<h32<bind_group_layout>> create_bind_group_layout(
            const bind_group_layout_descriptor& descriptor) override;

        void destroy_bind_group_layout(h32<bind_group_layout> handle) override;

        result<h32<bind_group>> acquire_transient_bind_group(const bind_group_descriptor& descriptor) override;

        result<h32<command_buffer_pool>> create_command_buffer_pool(
            const command_buffer_pool_descriptor& descriptor) override;

        void destroy_command_buffer_pool(h32<command_buffer_pool> commandBufferPool) override;

        result<> reset_command_buffer_pool(h32<command_buffer_pool> commandBufferPool) override;

        result<> fetch_command_buffers(h32<command_buffer_pool> pool,
            std::span<hptr<command_buffer>> commandBuffers) override;

        result<> begin_command_buffer(hptr<command_buffer> commandBuffer) override;
        result<> end_command_buffer(hptr<command_buffer> commandBuffer) override;

        result<h32<buffer>> create_buffer(const buffer_descriptor& descriptor) override;
        void destroy_buffer(h32<buffer> bufferHandle) override;

        h64<device_address> get_device_address(h32<buffer> bufferHandle) override;
        h64<device_address> get_device_address(buffer_range bufferWithOffset) override;

        result<h32<image>> create_image(const image_descriptor& descriptor) override;
        void destroy_image(h32<image> imageHandle) override;

        image_descriptor get_image_descriptor(h32<image> imageHandle) override;

        result<h32<image_pool>> create_image_pool(std::span<const image_descriptor> descriptors,
            std::span<h32<image>> images) override;

        void destroy_image_pool(h32<image_pool> imagePoolHandle) override;

        result<h32<shader_module>> create_shader_module(const shader_module_descriptor& descriptor) override;
        void destroy_shader_module(h32<shader_module> handle) override;

        result<h32<render_pipeline>> create_render_pipeline(const render_pipeline_descriptor& descriptor) override;
        void destroy_render_pipeline(h32<render_pipeline> handle) override;

        result<h32<sampler>> create_sampler(const sampler_descriptor& descriptor) override;
        void destroy_sampler(h32<sampler> handle) override;

        result<hptr<render_pass>> begin_render_pass(hptr<command_buffer> cmdBuffer,
            h32<render_pipeline> pipeline,
            const render_pass_descriptor& descriptor) override;

        void end_render_pass(hptr<command_buffer> cmdBuffer, hptr<render_pass> renderPass) override;

        result<h32<bindless_image>> acquire_bindless(h32<image> optImage) override;
        result<h32<bindless_image>> replace_bindless(h32<bindless_image> slot, h32<image> optImage) override;
        void release_bindless(h32<bindless_image> slot) override;

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

        // Barriers and synchronization

        void cmd_apply_barriers(hptr<command_buffer> cmd, const memory_barrier_descriptor& descriptor) override;

        // Debugging and profiling

        void cmd_label_begin(hptr<command_buffer> cmd, const char* label) override;
        void cmd_label_end(hptr<command_buffer> cmd) override;

        // Vulkan specific
        VkPhysicalDevice get_physical_device() const;
        VkDevice get_device() const;
        gpu_allocator& get_allocator();

        VkBuffer unwrap_buffer(h32<buffer> handle) const;
        VkCommandBuffer unwrap_command_buffer(hptr<command_buffer> handle) const;
        VkImage unwrap_image(h32<image> handle) const;
        VkImageView unwrap_image_view(h32<image> handle) const;
        VkQueue unwrap_queue(h32<queue> handle) const;
        VkShaderModule unwrap_shader_module(h32<shader_module> handle) const;

        debug_utils::object get_object_labeler() const;

        const loaded_functions& get_loaded_functions() const;

    private:
        struct bind_group_layout_impl;
        struct buffer_impl;
        struct command_buffer_pool_impl;
        struct image_impl;
        struct image_pool_impl;
        struct queue_impl;
        struct render_pipeline_impl;
        struct sampler_impl;
        struct shader_module_impl;
        struct swapchain_impl;

    private:
        h32<image> register_image(
            VkImage image, VkImageView view, VmaAllocation allocation, const image_descriptor& descriptor);

        void unregister_image(h32<image> image);

        const queue_impl& get_queue(h32<queue> queue) const;

        template <typename T>
        void label_vulkan_object(T obj, const debug_label& label);

    private:
        VkInstance m_instance{};
        VkPhysicalDevice m_physicalDevice{};
        VkDevice m_device{};
        gpu_allocator m_allocator;
        dynamic_array<queue_impl> m_queues;

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
        h32_flat_pool_dense_map<render_pipeline, render_pipeline_impl> m_renderPipelines;

        descriptor_set_pool m_descriptorSetPool;

        VkPhysicalDeviceProperties2 m_physicalDeviceProperties{};
        VkPhysicalDeviceSubgroupProperties m_subgroupProperties{};

        debug_utils::label m_cmdLabeler{};
        debug_utils::object m_objLabeler{};
        loaded_functions m_loadedFunctions{};
    };
}