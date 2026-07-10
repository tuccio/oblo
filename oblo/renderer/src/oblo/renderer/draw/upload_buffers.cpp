#include <oblo/renderer/draw/upload_buffers.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/span.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/scene/resources/texture.hpp>
#include <oblo/trace/profile.hpp>

namespace oblo
{
    template <typename F>
    inline gpu::result<gpu::staging_buffer_span> upload_buffers::stage_memory(F&& doStage)
    {
        OBLO_PROFILE_SCOPE();

        // Try staging first, if it fails, submit any pending upload to make space if needed
        // Either way we have to wait
        const expected firstAttempt = doStage();

        if (firstAttempt)
        {
            return firstAttempt.value();
        }

        submit_uploads().assert_value(
            "Failed to submit uploads for staging, this may not be critical but it's a worrying sign");

        while (true)
        {
            m_immediate.notify_finished_frames(m_gpu->get_last_finished_submit());
            const expected memorySpan = doStage();

            if (memorySpan)
            {
                return memorySpan.value();
            }

            const expected submit = m_immediate.get_first_pending_submit();

            if (!submit)
            {
                break;
            }

            m_gpu->wait_for_submit_completion(*submit).assert_value();
        }

        return gpu::error::out_of_memory;
    }

    gpu::result<> upload_buffers::init(gpu::gpu_instance& gpu, u64 size)
    {
        m_gpu = &gpu;

        if (const gpu::result r = m_immediate.init(gpu, size); !r)
        {
            return r.error();
        }

        m_commandPools.init(gpu, 1u);

        return no_error;
    }

    void upload_buffers::shutdown()
    {
        m_immediate.shutdown();
        m_commandPools.shutdown();
    }

    gpu::result<> upload_buffers::upload(std::span<const byte> source, h32<gpu::buffer> buffer, u64 bufferOffset)
    {
        OBLO_PROFILE_SCOPE();

        const expected staged = stage_memory([this, source] { return m_immediate.stage(source); });

        if (!staged)
        {
            return staged.error();
        }

        m_immediateUploads.push_back({
            .buffer = buffer,
            .bufferOffset = bufferOffset,
            .span = *staged,
        });

        return no_error;
    }

    gpu::result<> upload_buffers::upload(const texture& source, const immediate_image_upload_desc& uploadDesc)
    {
        const auto& srcDesc = source.get_description();

        // We may need to split the upload of 1 texture into multiple submits, we share some of the setup
        const staged_image_upload imageUpload{
            .image = uploadDesc.image,
            .format = gpu::image_format(srcDesc.vkFormat),
            .width = srcDesc.width,
            .height = srcDesc.height,
            .previousPipelines = uploadDesc.previousPipelines,
            .previousState = uploadDesc.previousState,
            .nextPipelines = uploadDesc.nextPipelines,
            .nextState = uploadDesc.nextState,
        };

        auto* textureUpload = &m_imageUploads.push_back(imageUpload);

        // We need a barrier for the first mip upload, and one after the last one
        textureUpload->isUploadBegin = true;
        textureUpload->isUploadEnd = false;
        textureUpload->stagedLevelBegin = m_imageLevels.size();
        textureUpload->stagedLevelEnd = m_imageLevels.size();

        const u32 texelSize = source.get_element_size();

        for (u32 levelIndex = 0; levelIndex < srcDesc.numLevels; ++levelIndex)
        {
            const auto data = source.get_data(levelIndex, 0, 0);

            const expected staged =
                stage_memory([this, texelSize, data] { return m_immediate.stage_image(data, texelSize); });

            if (!staged)
            {
                return staged.error();
            }

            // This would mean stage_memory flushed the uploads, we need to add a new one
            if (m_imageUploads.empty())
            {
                textureUpload = &m_imageUploads.push_back(imageUpload);
                textureUpload->isUploadBegin = false;
                textureUpload->isUploadEnd = false;
                textureUpload->stagedLevelBegin = m_imageLevels.size();
                textureUpload->stagedLevelEnd = m_imageLevels.size();
            }

            m_imageLevels.push_back({
                .span = *staged,
                .levelIndex = levelIndex,
            });

            textureUpload->stagedLevelEnd = m_imageLevels.size();
        }

        textureUpload->isUploadEnd = true;

        return no_error;
    }

    gpu::result<> upload_buffers::submit_uploads()
    {
        if (m_immediateUploads.empty() && m_imageUploads.empty())
        {
            return no_error;
        }

        auto& gpu = *m_gpu;

        const expected cmdBuffer = m_commandPools.allocate_command_buffer();

        if (!cmdBuffer)
        {
            return cmdBuffer.error();
        }

        if (const auto e = gpu.begin_command_buffer(*cmdBuffer); !e)
        {
            return e.error();
        }

        if (!m_immediateUploads.empty())
        {
            const gpu::global_memory_barrier before[] = {
                {
                    .previousPipelines = gpu::pipeline_sync_stage::all_commands,
                    .previousAccesses = gpu::memory_access_type::any_read | gpu::memory_access_type::any_write,
                    .nextPipelines = gpu::pipeline_sync_stage::transfer,
                    .nextAccesses = gpu::memory_access_type::any_write,
                },
            };

            gpu.cmd_label_begin(*cmdBuffer, "update_buffers::submit_uploads");
            gpu.cmd_apply_barriers(*cmdBuffer, {.memory = before});

            for (const auto& upload : m_immediateUploads)
            {
                m_immediate.upload(*cmdBuffer, upload.span, upload.buffer, upload.bufferOffset);
            }

            const gpu::global_memory_barrier after[] = {
                {
                    .previousPipelines = gpu::pipeline_sync_stage::transfer,
                    .previousAccesses = gpu::memory_access_type::any_write,
                    .nextPipelines = gpu::pipeline_sync_stage::all_commands,
                    .nextAccesses = gpu::memory_access_type::any_read,
                },
            };

            gpu.cmd_apply_barriers(*cmdBuffer, {.memory = after});
            gpu.cmd_label_end(*cmdBuffer);

            m_immediateUploads.clear();
        }

        if (!m_imageUploads.empty())
        {
            buffered_array<gpu::buffer_image_copy_descriptor, 16> copies;

            for (const auto& imageUpload : m_imageUploads)
            {
                copies.clear();
                copies.reserve(imageUpload.stagedLevelEnd - imageUpload.stagedLevelBegin);

                for (usize stagedLevelIndex = imageUpload.stagedLevelBegin;
                    stagedLevelIndex < imageUpload.stagedLevelEnd;
                    ++stagedLevelIndex)
                {
                    const auto& level = m_imageLevels[stagedLevelIndex];
                    const auto segment = level.span.segments[0];

                    OBLO_ASSERT(level.span.segments[1].end == level.span.segments[1].begin);

                    copies.push_back(gpu::buffer_image_copy_descriptor{
                        .bufferOffset = segment.begin,
                        .imageSubresource =
                            {
                                .mipLevel = level.levelIndex,
                                .baseArrayLayer = 0,
                                .layerCount = 1,
                            },
                        .imageOffset = {},
                        .imageExtent =
                            {
                                imageUpload.width >> level.levelIndex,
                                imageUpload.height >> level.levelIndex,
                                1,
                            },
                    });
                }

                if (imageUpload.isUploadBegin)
                {
                    gpu.cmd_apply_barriers(*cmdBuffer,
                        gpu::memory_barrier_descriptor{
                            .images = make_span_initializer<gpu::image_state_transition>({
                                {
                                    .image = imageUpload.image,
                                    .previousPipelines = imageUpload.previousPipelines,
                                    .previousState = imageUpload.previousState,
                                    .nextPipelines = gpu::pipeline_sync_stage::transfer,
                                    .nextState = gpu::image_resource_state::transfer_destination,
                                },
                            }),
                        });
                }

                if (!copies.empty())
                {
                    m_immediate.upload(*cmdBuffer, imageUpload.image, copies);
                }

                if (imageUpload.isUploadEnd)
                {
                    gpu.cmd_apply_barriers(*cmdBuffer,
                        gpu::memory_barrier_descriptor{
                            .images = make_span_initializer<gpu::image_state_transition>({
                                {
                                    .image = imageUpload.image,
                                    .previousPipelines = gpu::pipeline_sync_stage::transfer,
                                    .previousState = gpu::image_resource_state::transfer_destination,
                                    .nextPipelines = imageUpload.nextPipelines,
                                    .nextState = imageUpload.nextState,
                                },
                            }),
                        });
                }
            }

            m_imageUploads.clear();
            m_imageLevels.clear();
        }

        gpu.end_command_buffer(*cmdBuffer).assert_value();

        const expected submitIndex = gpu.submit(gpu.get_universal_queue(),
            {
                .commandBuffers = {&*cmdBuffer, 1u},
            });

        if (!submitIndex)
        {
            return submitIndex.error();
        }

        m_commandPools.frame_submitted(*submitIndex);
        m_immediate.end_submit(*submitIndex);

        return no_error;
    }

}