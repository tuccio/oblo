#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/gpu/command_buffer_pool_manager.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/error.hpp>
#include <oblo/gpu/staging_buffer.hpp>

namespace oblo
{
    class texture;

    struct immediate_image_upload_desc;

    class upload_buffers
    {
    public:
        gpu::result<> init(gpu::gpu_instance& gpu, u64 size);
        void shutdown();

        gpu::result<> submit_uploads();

        gpu::result<> upload(std::span<const byte> source, h32<gpu::buffer> buffer, u64 bufferOffset);

        gpu::result<> upload(const texture& source, const immediate_image_upload_desc& desc);

    private:
        struct staged_buffer_upload
        {
            h32<gpu::buffer> buffer;
            u64 bufferOffset;
            gpu::staging_buffer_span span;
        };

        struct staged_image_upload
        {
            h32<gpu::image> image;
            gpu::image_format format;
            u32 width;
            u32 height;
            bool isUploadBegin;
            bool isUploadEnd;
            flags<gpu::pipeline_sync_stage> previousPipelines;
            gpu::image_resource_state previousState;
            flags<gpu::pipeline_sync_stage> nextPipelines;
            gpu::image_resource_state nextState;

            usize stagedLevelBegin;
            usize stagedLevelEnd;
        };

        struct staged_image_level
        {
            gpu::staging_buffer_span span;
            u32 levelIndex;
        };

    private:
        template <typename F>
        gpu::result<gpu::staging_buffer_span> stage_memory(F&& doStage);

    private:
        gpu::gpu_instance* m_gpu{};
        gpu::staging_buffer m_immediate;
        gpu::command_buffer_pool_manager m_commandPools;
        dynamic_array<staged_buffer_upload> m_immediateUploads;
        dynamic_array<staged_image_upload> m_imageUploads;
        dynamic_array<staged_image_level> m_imageLevels;
    };

    struct immediate_image_upload_desc
    {
        h32<gpu::image> image;
        flags<gpu::pipeline_sync_stage> previousPipelines;
        gpu::image_resource_state previousState;
        flags<gpu::pipeline_sync_stage> nextPipelines;
        gpu::image_resource_state nextState;
    };
}