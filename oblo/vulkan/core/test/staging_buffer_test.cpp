#include <gtest/gtest.h>

#include <oblo/core/finally.hpp>
#include <oblo/vulkan/allocator.hpp>
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

            allocator allocator;
            ASSERT_TRUE(allocator.init(sandbox.instance.get(),
                sandbox.engine.get_physical_device(),
                sandbox.engine.get_device()));

            staging_buffer stagingBuffer;
            ASSERT_TRUE(stagingBuffer.init(sandbox.engine, allocator, stagingBufferSize));

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
                [&allocator, &buffers]
                {
                    for (auto buffer : buffers)
                    {
                        allocator.destroy(buffer);
                    }
                });

            using data_array = std::array<i32, bufferSize / sizeof(i32)>;

            data_array data;
            std::iota(std::begin(data), std::end(data), 0);

            const auto dataSpan = std::as_bytes(std::span{data});

            // The first uploads will work, after that we need to flush because we used the whole staging buffer
            ASSERT_TRUE(stagingBuffer.upload(dataSpan, buffers[0].buffer, 0));
            ASSERT_TRUE(stagingBuffer.upload(dataSpan, buffers[1].buffer, 0));
            ASSERT_FALSE(stagingBuffer.upload(dataSpan, buffers[2].buffer, 0));
            ASSERT_FALSE(stagingBuffer.upload(dataSpan, buffers[3].buffer, 0));

            stagingBuffer.flush();
            vkQueueWaitIdle(sandbox.engine.get_queue());

            ASSERT_VK_SUCCESS(allocator.invalidate_mapped_memory_ranges(allocations));

            auto readbackBuffer = [](void* mapping)
            {
                data_array readback;
                std::memcpy(&readback, mapping, sizeof(readback));
                return readback;
            };

            ASSERT_EQ(readbackBuffer(mappings[0]), data);
            ASSERT_EQ(readbackBuffer(mappings[1]), data);

            stagingBuffer.wait_for_free_space(stagingBufferSize);

            // Now we do it the other way around, so it will be 0 and 1 to fail
            ASSERT_TRUE(stagingBuffer.upload(dataSpan, buffers[3].buffer, 0));
            ASSERT_TRUE(stagingBuffer.upload(dataSpan, buffers[2].buffer, 0));
            ASSERT_FALSE(stagingBuffer.upload(dataSpan, buffers[1].buffer, 0));
            ASSERT_FALSE(stagingBuffer.upload(dataSpan, buffers[0].buffer, 0));

            stagingBuffer.flush();
            vkQueueWaitIdle(sandbox.engine.get_queue());

            ASSERT_VK_SUCCESS(allocator.invalidate_mapped_memory_ranges(allocations));

            ASSERT_EQ(readbackBuffer(mappings[2]), data);
            ASSERT_EQ(readbackBuffer(mappings[3]), data);

            stagingBuffer.wait_for_free_space(stagingBufferSize);

            std::array<i32, data_array{}.size() / 2> halfSizeData;
            std::fill(std::begin(halfSizeData), std::end(halfSizeData), 42);

            const auto newDataSpan = std::as_bytes(std::span{halfSizeData});

            data_array newExpected = data;
            std::copy(halfSizeData.begin(), halfSizeData.end(), newExpected.begin());

            ASSERT_TRUE(stagingBuffer.upload(newDataSpan, buffers[0].buffer, 0));
            ASSERT_TRUE(stagingBuffer.upload(newDataSpan, buffers[1].buffer, 0));
            ASSERT_TRUE(stagingBuffer.upload(newDataSpan, buffers[2].buffer, 0));

            stagingBuffer.flush();
            vkQueueWaitIdle(sandbox.engine.get_queue());

            ASSERT_VK_SUCCESS(allocator.invalidate_mapped_memory_ranges(allocations));

            ASSERT_EQ(newExpected, readbackBuffer(mappings[0]));
            ASSERT_EQ(newExpected, readbackBuffer(mappings[1]));
            ASSERT_EQ(newExpected, readbackBuffer(mappings[2]));
            ASSERT_NE(newExpected, readbackBuffer(mappings[3]));

            ASSERT_FALSE(stagingBuffer.upload(std::as_bytes(std::span{newExpected}), buffers[3].buffer, 0));

            stagingBuffer.wait_for_free_space(bufferSize);

            ASSERT_TRUE(stagingBuffer.upload(std::as_bytes(std::span{newExpected}), buffers[3].buffer, 0));

            stagingBuffer.flush();
            vkQueueWaitIdle(sandbox.engine.get_queue());

            ASSERT_VK_SUCCESS(allocator.invalidate_mapped_memory_ranges(allocations));

            ASSERT_EQ(newExpected, readbackBuffer(mappings[3]));
        }

        const auto errorsString = validationErrors.str();
        ASSERT_TRUE(errorsString.empty()) << errorsString;
    }
}