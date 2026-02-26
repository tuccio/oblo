#include <gtest/gtest.h>

#include <oblo/core/finally.hpp>
#include <oblo/core/random_generator.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/staging_buffer.hpp>

#include <oblo/gpu/vulkan/vulkan_instance.hpp>

#include <array>
#include <numeric>

namespace oblo::gpu
{
    namespace
    {
        unique_ptr<gpu_instance> create_instance(const char* name)
        {
            auto gpu = allocate_unique<vk::vulkan_instance>();

            if (!gpu->init({.application = name, .engine = "oblo"}))
            {
                return {};
            }

            return gpu;
        }
    }

    TEST(staging_buffer, staging_buffer)
    {
        constexpr u32 bufferSize{64u};
        constexpr u32 stagingBufferSize{128u};
        constexpr u32 buffersCount{4};

        unique_ptr gpu = create_instance("staging_buffer_test");

        const auto cleanup = finally([&] { gpu->shutdown(); });

        ASSERT_TRUE(gpu->finalize_init(
            {
                .requireHardwareRaytracing = false,
            },
            {}));

        staging_buffer stagingBuffer;
        ASSERT_TRUE(stagingBuffer.init(*gpu, stagingBufferSize));

        const h32 universalQueue = gpu->get_universal_queue();

        const h32 pool = gpu->create_command_buffer_pool(command_buffer_pool_descriptor{
                                                             .queue = universalQueue,
                                                             .numCommandBuffers = 1,
                                                         })
                             .value_or({});

        ASSERT_TRUE(pool);

        const h32 fence = gpu->create_fence({}).value_or({});
        ASSERT_TRUE(fence);

        // Create the buffers
        h32<buffer> buffers[buffersCount];
        void* mappings[buffersCount];

        for (u32 index = 0u; index < buffersCount; ++index)
        {
            buffers[index] = gpu->create_buffer({
                                                    .size = bufferSize,
                                                    .memoryProperties = {memory_usage::gpu_to_cpu},
                                                    .usages = buffer_usage::transfer_destination,
                                                })
                                 .value_or({});

            mappings[index] = gpu->memory_map(buffers[index]).value_or({});

            ASSERT_TRUE(buffers[index]);
            ASSERT_TRUE(mappings[index]);
        }

        const auto testCleanup = finally(
            [&]
            {
                ASSERT_TRUE(gpu->wait_idle());

                gpu->destroy(pool);
                gpu->destroy(fence);

                for (auto buffer : buffers)
                {
                    ASSERT_TRUE(gpu->memory_unmap(buffer));
                    gpu->destroy(buffer);
                }
            });

        using data_array = std::array<i32, bufferSize / sizeof(i32)>;

        data_array data;
        std::iota(std::begin(data), std::end(data), 0);

        const std::span dataSpan = std::as_bytes(std::span{data});

        {
            // The first 2 uploads will work, after that we need to flush because we used the whole staging buffer
            constexpr u64 frameIndex{1};

            stagingBuffer.begin_frame(frameIndex);

            const expected<staging_buffer_span> staged[] = {
                stagingBuffer.stage(dataSpan),
                stagingBuffer.stage(dataSpan),
                stagingBuffer.stage(dataSpan),
                stagingBuffer.stage(dataSpan),
            };

            ASSERT_TRUE(staged[0]);
            ASSERT_TRUE(staged[1]);
            ASSERT_FALSE(staged[2]);
            ASSERT_FALSE(staged[3]);

            hptr<command_buffer> commandBuffer;
            ASSERT_TRUE(gpu->fetch_command_buffers(pool, {&commandBuffer, 1}));

            ASSERT_TRUE(gpu->begin_command_buffer(commandBuffer));

            stagingBuffer.upload(commandBuffer, *staged[0], buffers[0], 0);
            stagingBuffer.upload(commandBuffer, *staged[1], buffers[1], 0);

            stagingBuffer.end_frame();

            ASSERT_TRUE(gpu->end_command_buffer(commandBuffer));

            const queue_submit_descriptor submit{.commandBuffers = {&commandBuffer, 1}, .signalFence = fence};

            ASSERT_TRUE(gpu->submit(universalQueue, submit));
        }

        ASSERT_TRUE(gpu->wait_for_fences({&fence, 1}));
        ASSERT_TRUE(gpu->reset_fences({&fence, 1}));

        ASSERT_TRUE(gpu->memory_invalidate({buffers, buffersCount}));

        auto readbackBuffer = [](void* mapping)
        {
            data_array readback;
            std::memcpy(&readback, mapping, sizeof(readback));
            return readback;
        };

        ASSERT_EQ(readbackBuffer(mappings[0]), data);
        ASSERT_EQ(readbackBuffer(mappings[1]), data);

        {
            // Now we do it the other way around, upload to 2 and 3
            constexpr u64 frameIndex{2};

            stagingBuffer.notify_finished_frames(frameIndex - 1);
            stagingBuffer.begin_frame(frameIndex);
            ASSERT_TRUE(gpu->reset_command_buffer_pool(pool));

            const expected<staging_buffer_span> staged[] = {
                stagingBuffer.stage(dataSpan),
                stagingBuffer.stage(dataSpan),
                stagingBuffer.stage(dataSpan),
                stagingBuffer.stage(dataSpan),
            };

            // The first uploads will work, after that we need to flush because we used the whole staging buffer
            ASSERT_TRUE(staged[0]);
            ASSERT_TRUE(staged[1]);
            ASSERT_FALSE(staged[2]);
            ASSERT_FALSE(staged[3]);

            hptr<command_buffer> commandBuffer;
            ASSERT_TRUE(gpu->fetch_command_buffers(pool, {&commandBuffer, 1}));

            ASSERT_TRUE(gpu->begin_command_buffer(commandBuffer));

            stagingBuffer.upload(commandBuffer, *staged[0], buffers[2], 0);
            stagingBuffer.upload(commandBuffer, *staged[1], buffers[3], 0);

            stagingBuffer.end_frame();

            ASSERT_TRUE(gpu->end_command_buffer(commandBuffer));

            const queue_submit_descriptor submit{.commandBuffers = {&commandBuffer, 1}, .signalFence = fence};

            ASSERT_TRUE(gpu->submit(universalQueue, submit));
        }

        ASSERT_TRUE(gpu->wait_for_fences({&fence, 1}));
        ASSERT_TRUE(gpu->reset_fences({&fence, 1}));

        ASSERT_TRUE(gpu->memory_invalidate({buffers, buffersCount}));

        ASSERT_EQ(readbackBuffer(mappings[2]), data);
        ASSERT_EQ(readbackBuffer(mappings[3]), data);

        // Prepare some data to overwrite the buffers with
        std::array<i32, data_array{}.size() / 2> halfSizeData;
        std::fill(std::begin(halfSizeData), std::end(halfSizeData), 42);

        const auto newDataSpan = std::as_bytes(std::span{halfSizeData});

        data_array newExpected = data;
        std::copy(halfSizeData.begin(), halfSizeData.end(), newExpected.begin());

        {
            // Now upload different data to 0, 1 and 2, check that 3 is still the same as before and the others are
            // updated
            constexpr u64 frameIndex{3};

            stagingBuffer.notify_finished_frames(frameIndex - 1);
            stagingBuffer.begin_frame(frameIndex);
            ASSERT_TRUE(gpu->reset_command_buffer_pool(pool));

            const expected<staging_buffer_span> staged[] = {
                stagingBuffer.stage(newDataSpan),
                stagingBuffer.stage(newDataSpan),
                stagingBuffer.stage(newDataSpan),
            };

            ASSERT_TRUE(staged[0]);
            ASSERT_TRUE(staged[1]);
            ASSERT_TRUE(staged[2]);

            hptr<command_buffer> commandBuffer{};
            ASSERT_TRUE(gpu->fetch_command_buffers(pool, {&commandBuffer, 1}));

            ASSERT_TRUE(gpu->begin_command_buffer(commandBuffer));

            stagingBuffer.upload(commandBuffer, *staged[0], buffers[0], 0);
            stagingBuffer.upload(commandBuffer, *staged[1], buffers[1], 0);
            stagingBuffer.upload(commandBuffer, *staged[2], buffers[2], 0);

            stagingBuffer.end_frame();

            ASSERT_TRUE(gpu->end_command_buffer(commandBuffer));

            const queue_submit_descriptor submit{.commandBuffers = {&commandBuffer, 1}, .signalFence = fence};

            ASSERT_TRUE(gpu->submit(universalQueue, submit));
        }

        ASSERT_TRUE(gpu->wait_for_fences({&fence, 1}));
        ASSERT_TRUE(gpu->reset_fences({&fence, 1}));

        ASSERT_TRUE(gpu->memory_invalidate({buffers, buffersCount}));

        ASSERT_EQ(newExpected, readbackBuffer(mappings[0]));
        ASSERT_EQ(newExpected, readbackBuffer(mappings[1]));
        ASSERT_EQ(newExpected, readbackBuffer(mappings[2]));
        ASSERT_NE(newExpected, readbackBuffer(mappings[3]));
    }
}