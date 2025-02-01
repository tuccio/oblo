#include <gtest/gtest.h>

#include <oblo/core/finally.hpp>
#include <oblo/core/random_generator.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/staging_buffer.hpp>

#include <array>
#include <numeric>

#include "test_sandbox.hpp"

namespace oblo::vk
{
    TEST(staging_buffer, staging_buffer)
    {
        constexpr u32 bufferSize{64u};
        constexpr u32 stagingBufferSize{128u};
        constexpr u32 buffersCount{4};

        std::stringstream validationErrors;

        {
            test_sandbox sandbox;
            ASSERT_TRUE(sandbox.init({}, {}, {}, nullptr, nullptr, &validationErrors));

            gpu_allocator allocator;
            ASSERT_TRUE(allocator.init(sandbox.instance.get(),
                sandbox.engine.get_physical_device(),
                sandbox.engine.get_device()));

            staging_buffer stagingBuffer;
            ASSERT_TRUE(stagingBuffer.init(allocator, stagingBufferSize));

            command_buffer_pool commandBufferPool;
            commandBufferPool.init(sandbox.engine.get_device(), sandbox.engine.get_queue_family_index(), true, 1, 1);

            VkFence fence{};

            constexpr VkFenceCreateInfo fenceInfo{
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0u,
            };

            ASSERT_EQ(
                vkCreateFence(sandbox.engine.get_device(), &fenceInfo, allocator.get_allocation_callbacks(), &fence),
                VK_SUCCESS);

            // Create the buffers
            allocated_buffer buffers[buffersCount];
            void* mappings[buffersCount];
            VmaAllocation allocations[buffersCount];

            for (u32 index = 0u; index < buffersCount; ++index)
            {
                ASSERT_VK_SUCCESS(allocator.create_buffer(
                    {
                        .size = bufferSize,
                        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        .memoryUsage = memory_usage::gpu_to_cpu,
                    },
                    buffers + index));

                ASSERT_VK_SUCCESS(allocator.map(buffers[index].allocation, mappings + index));

                allocations[index] = buffers[index].allocation;
            }

            const auto cleanup = finally(
                [&]
                {
                    vkDeviceWaitIdle(sandbox.engine.get_device());

                    commandBufferPool.shutdown();

                    if (fence)
                    {
                        vkDestroyFence(sandbox.engine.get_device(), fence, allocator.get_allocation_callbacks());
                    }

                    for (auto buffer : buffers)
                    {
                        allocator.unmap(buffer.allocation);
                        allocator.destroy(buffer);
                    }
                });

            using data_array = std::array<i32, bufferSize / sizeof(i32)>;

            data_array data;
            std::iota(std::begin(data), std::end(data), 0);

            const auto dataSpan = std::as_bytes(std::span{data});

            {
                // The first 2 uploads will work, after that we need to flush because we used the whole staging buffer
                constexpr u64 frameIndex{1};

                stagingBuffer.begin_frame(frameIndex);
                commandBufferPool.begin_frame(frameIndex);

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

                const auto commandBuffer = commandBufferPool.fetch_buffer();

                const VkCommandBufferBeginInfo commandBufferBeginInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                };

                vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

                stagingBuffer.upload(commandBuffer, *staged[0], buffers[0].buffer, 0);
                stagingBuffer.upload(commandBuffer, *staged[1], buffers[1].buffer, 0);

                stagingBuffer.end_frame();

                vkEndCommandBuffer(commandBuffer);

                const VkSubmitInfo submit{
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &commandBuffer,
                };

                vkQueueSubmit(sandbox.engine.get_queue(), 1, &submit, fence);
            }

            vkWaitForFences(sandbox.engine.get_device(), 1, &fence, VK_TRUE, ~u64{});
            vkResetFences(sandbox.engine.get_device(), 1, &fence);

            ASSERT_VK_SUCCESS(allocator.invalidate_mapped_memory_ranges(allocations));

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
                commandBufferPool.reset_buffers(frameIndex);
                commandBufferPool.begin_frame(frameIndex);

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

                const auto commandBuffer = commandBufferPool.fetch_buffer();

                const VkCommandBufferBeginInfo commandBufferBeginInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                };

                vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

                stagingBuffer.upload(commandBuffer, *staged[0], buffers[2].buffer, 0);
                stagingBuffer.upload(commandBuffer, *staged[1], buffers[3].buffer, 0);

                stagingBuffer.end_frame();

                vkEndCommandBuffer(commandBuffer);

                const VkSubmitInfo submit{
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &commandBuffer,
                };

                vkQueueSubmit(sandbox.engine.get_queue(), 1, &submit, fence);
            }

            vkWaitForFences(sandbox.engine.get_device(), 1, &fence, VK_TRUE, ~u64{});
            vkResetFences(sandbox.engine.get_device(), 1, &fence);

            ASSERT_VK_SUCCESS(allocator.invalidate_mapped_memory_ranges(allocations));

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
                commandBufferPool.reset_buffers(frameIndex);
                commandBufferPool.begin_frame(frameIndex);

                const expected<staging_buffer_span> staged[] = {
                    stagingBuffer.stage(newDataSpan),
                    stagingBuffer.stage(newDataSpan),
                    stagingBuffer.stage(newDataSpan),
                };

                ASSERT_TRUE(staged[0]);
                ASSERT_TRUE(staged[1]);
                ASSERT_TRUE(staged[2]);

                const auto commandBuffer = commandBufferPool.fetch_buffer();

                const VkCommandBufferBeginInfo commandBufferBeginInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                };

                vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

                stagingBuffer.upload(commandBuffer, *staged[0], buffers[0].buffer, 0);
                stagingBuffer.upload(commandBuffer, *staged[1], buffers[1].buffer, 0);
                stagingBuffer.upload(commandBuffer, *staged[2], buffers[2].buffer, 0);

                stagingBuffer.end_frame();

                vkEndCommandBuffer(commandBuffer);

                const VkSubmitInfo submit{
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &commandBuffer,
                };

                vkQueueSubmit(sandbox.engine.get_queue(), 1, &submit, fence);
            }

            vkWaitForFences(sandbox.engine.get_device(), 1, &fence, VK_TRUE, ~u64{});
            vkResetFences(sandbox.engine.get_device(), 1, &fence);

            ASSERT_VK_SUCCESS(allocator.invalidate_mapped_memory_ranges(allocations));

            ASSERT_EQ(newExpected, readbackBuffer(mappings[0]));
            ASSERT_EQ(newExpected, readbackBuffer(mappings[1]));
            ASSERT_EQ(newExpected, readbackBuffer(mappings[2]));
            ASSERT_NE(newExpected, readbackBuffer(mappings[3]));
        }

        const auto errorsString = validationErrors.str();
        ASSERT_TRUE(errorsString.empty()) << errorsString;
    }

    TEST(staging_buffer, randomized)
    {
        constexpr u32 bufferSize{1 << 8u};
        constexpr u32 stagingBufferSize{bufferSize};

        std::stringstream validationErrors;

        {
            test_sandbox sandbox;
            ASSERT_TRUE(sandbox.init({}, {}, {}, nullptr, nullptr, &validationErrors));

            gpu_allocator allocator;
            ASSERT_TRUE(allocator.init(sandbox.instance.get(),
                sandbox.engine.get_physical_device(),
                sandbox.engine.get_device()));

            staging_buffer stagingBuffer;
            ASSERT_TRUE(stagingBuffer.init(allocator, stagingBufferSize));

            command_buffer_pool commandBufferPool;
            commandBufferPool.init(sandbox.engine.get_device(), sandbox.engine.get_queue_family_index(), true, 1, 1);

            VkFence fence{};

            constexpr VkFenceCreateInfo fenceInfo{
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0u,
            };

            ASSERT_EQ(
                vkCreateFence(sandbox.engine.get_device(), &fenceInfo, allocator.get_allocation_callbacks(), &fence),
                VK_SUCCESS);

            // Create the buffers
            allocated_buffer buffer;
            void* mapping;

            ASSERT_VK_SUCCESS(allocator.create_buffer(
                {
                    .size = bufferSize,
                    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .memoryUsage = memory_usage::gpu_to_cpu,
                },
                &buffer));

            ASSERT_VK_SUCCESS(allocator.map(buffer.allocation, &mapping));

            const auto cleanup = finally(
                [&]
                {
                    vkDeviceWaitIdle(sandbox.engine.get_device());

                    commandBufferPool.shutdown();

                    if (fence)
                    {
                        vkDestroyFence(sandbox.engine.get_device(), fence, allocator.get_allocation_callbacks());
                    }

                    allocator.unmap(buffer.allocation);
                    allocator.destroy(buffer);
                });

            random_generator rng;
            rng.seed();

            dynamic_array<u8> data;
            data.reserve(bufferSize);

            uniform_distribution<u32> countDist{1, bufferSize};
            uniform_distribution<u32> valueDist;

            for (u32 frameIndex = 1; frameIndex < 1000; ++frameIndex)
            {
                const auto n = countDist(rng);

                data.clear();
                data.resize_default(n);

                for (u32 j = 0; j < n; ++j)
                {
                    data[j] = u8(valueDist(rng));
                }

                const auto r = stagingBuffer.stage(as_bytes(std::span{data}));
                ASSERT_TRUE(r);

                commandBufferPool.begin_frame(frameIndex);
                stagingBuffer.begin_frame(frameIndex);

                const auto commandBuffer = commandBufferPool.fetch_buffer();

                const VkCommandBufferBeginInfo commandBufferBeginInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                };

                vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

                stagingBuffer.upload(commandBuffer, *r, buffer.buffer, 0);
                stagingBuffer.end_frame();

                vkEndCommandBuffer(commandBuffer);

                const VkSubmitInfo submit{
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &commandBuffer,
                };

                ASSERT_VK_SUCCESS(vkQueueSubmit(sandbox.engine.get_queue(), 1, &submit, fence));

                ASSERT_VK_SUCCESS(vkWaitForFences(sandbox.engine.get_device(), 1, &fence, VK_TRUE, ~u64{}));
                ASSERT_VK_SUCCESS(vkResetFences(sandbox.engine.get_device(), 1, &fence));

                stagingBuffer.notify_finished_frames(frameIndex);
                commandBufferPool.reset_buffers(frameIndex + 1);

                ASSERT_VK_SUCCESS(allocator.invalidate_mapped_memory_ranges({&buffer.allocation, 1}));

                ASSERT_EQ(std::memcmp(data.data(), mapping, data.size()), 0);
            }
        }

        const auto errorsString = validationErrors.str();
        ASSERT_TRUE(errorsString.empty()) << errorsString;
    }
}