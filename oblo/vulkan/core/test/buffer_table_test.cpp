#include <gtest/gtest.h>

#include <oblo/core/array_size.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/buffer_table.hpp>
#include <oblo/vulkan/dynamic_array_buffer.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/staging_buffer.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

#include <array>
#include <numeric>

#include "test_sandbox.hpp"

namespace oblo::vk
{
    TEST(buffer_table, buffer_table)
    {

        std::stringstream validationErrors;

        {
            test_sandbox sandbox;
            ASSERT_TRUE(sandbox.init({}, {}, {}, nullptr, nullptr, &validationErrors));

            gpu_allocator allocator;
            ASSERT_TRUE(allocator.init(sandbox.instance.get(),
                sandbox.engine.get_physical_device(),
                sandbox.engine.get_device()));

            string_interner interner;
            interner.init(16);
            const auto position = interner.get_or_add("position");
            const auto uv0 = interner.get_or_add("uv0");
            const auto uv1 = interner.get_or_add("uv1");
            const auto color = interner.get_or_add("color");
            const auto normal = interner.get_or_add("normal");

            resource_manager resourceManager;

            buffer_table::column_description columns[] = {
                {.name = uv1, .elementSize = sizeof(vec2)},
                {.name = uv0, .elementSize = sizeof(vec2)},
                {.name = position, .elementSize = sizeof(vec3)},
                {.name = normal, .elementSize = sizeof(vec3)},
                {.name = color, .elementSize = sizeof(vec3)},
            };

            const auto numColumns = array_size(columns);

            constexpr auto compareColumnsByName =
                [](const buffer_table::column_description& lhs, const buffer_table::column_description& rhs)
            { return lhs.name < rhs.name; };

            ASSERT_FALSE(std::is_sorted(std::begin(columns), std::end(columns), compareColumnsByName));

            constexpr u32 numRows = 42;

            allocated_buffer allocatedBuffer;

            constexpr u32 bufferSize = 128u << 20;

            ASSERT_VK_SUCCESS(allocator.create_buffer(
                {.size = bufferSize, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, .memoryUsage = memory_usage::gpu_only},
                &allocatedBuffer));

            buffer_table bufferTable;

            const u32 allocated = bufferTable.init(
                {
                    .buffer = allocatedBuffer.buffer,
                    .offset = 0,
                    .size = bufferSize,
                    .allocation = allocatedBuffer.allocation,
                },
                columns,
                resourceManager,
                numRows,
                16u);

            ASSERT_GT(allocated, 0);

            const std::span bufferColumns = bufferTable.buffers();
            const std::span nameColumns = bufferTable.names();
            const std::span elementSizes = bufferTable.element_sizes();

            ASSERT_EQ(numColumns, bufferColumns.size());
            ASSERT_EQ(numColumns, nameColumns.size());

            ASSERT_TRUE(std::is_sorted(std::begin(nameColumns), std::end(nameColumns)));

            for (const auto& column : columns)
            {
                const auto index = bufferTable.find(column.name);
                ASSERT_GE(index, 0);

                ASSERT_EQ(nameColumns[index], column.name);
                ASSERT_EQ(elementSizes[index], column.elementSize);
                ASSERT_TRUE(bufferColumns[index]);

                const auto buffer = resourceManager.get(bufferColumns[index]);
                ASSERT_EQ(buffer.size, numRows * column.elementSize);
            }

            bufferTable.shutdown(resourceManager);

            allocator.destroy(allocatedBuffer);
        }

        const auto errorsString = validationErrors.str();
        ASSERT_TRUE(errorsString.empty()) << errorsString;
    }

    TEST(dynamic_array_buffer, dynamic_array_buffer)
    {
        std::stringstream validationErrors;

        test_sandbox sandbox;
        ASSERT_TRUE(sandbox.init({}, {}, {}, nullptr, nullptr, &validationErrors));

        gpu_allocator allocator;
        ASSERT_TRUE(
            allocator.init(sandbox.instance.get(), sandbox.engine.get_physical_device(), sandbox.engine.get_device()));

        resource_manager resourceManager;

        vulkan_context ctx;

        ASSERT_TRUE(ctx.init({
            .instance = sandbox.instance.get(),
            .engine = sandbox.engine,
            .allocator = allocator,
            .resourceManager = resourceManager,
            .buffersPerFrame = 4,
            .submitsInFlight = 2,
        }));

        staging_buffer staging;
        ASSERT_TRUE(staging.init(allocator, 1u << 30));

        std::vector<u32> cpuData;
        std::vector<u32> downloadedData;

        dynamic_array_buffer buffer;
        buffer.init<u32>(ctx, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0);

        const auto cleanup = finally(
            [&]
            {
                buffer.clear_and_shrink();
                ctx.shutdown();
            });

        const auto submitAndCheckEquality = [&]
        {
            ctx.frame_begin(nullptr, nullptr);

            downloadedData.assign(cpuData.size(), 0xdeadbeef);

            auto bytes = std::as_writable_bytes(std::span{downloadedData});

            const auto vkBuffer = buffer.get_buffer();

            const auto result = staging.stage_allocate(u32(bytes.size()));

            if (!result)
            {
                return false;
            }

            staging.begin_frame(1);
            staging.download(ctx.get_active_command_buffer().get(), vkBuffer.buffer, vkBuffer.offset, *result);
            staging.end_frame();

            ctx.submit_active_command_buffer();
            ctx.frame_end();

            vkDeviceWaitIdle(ctx.get_device());

            staging.copy_from(bytes, *result, 0);

            return downloadedData == cpuData;
        };

        for (u32 i = 0; i < 10; ++i)
        {
            {
                ctx.frame_begin(nullptr, nullptr);

                const u32 newSize = 1u << (i + 1);

                cpuData.resize(newSize);
                buffer.resize(ctx.get_active_command_buffer().get(), newSize);

                ctx.frame_end();

                vkDeviceWaitIdle(ctx.get_device());
            }

            ASSERT_TRUE(submitAndCheckEquality());

            {
                std::iota(cpuData.begin(), cpuData.end(), 0);

                const auto vkBuffer = buffer.get_buffer();

                const auto staged = staging.stage(std::as_bytes(std::span{cpuData}));
                ASSERT_TRUE(staged);

                ctx.frame_begin(nullptr, nullptr);

                staging.begin_frame(2);
                staging.upload(ctx.get_active_command_buffer().get(), *staged, vkBuffer.buffer, vkBuffer.offset);
                staging.end_frame();

                ctx.submit_active_command_buffer();

                ctx.frame_end();

                vkDeviceWaitIdle(ctx.get_device());
            }

            ASSERT_TRUE(submitAndCheckEquality());
        }

        const auto errorsString = validationErrors.str();
        ASSERT_TRUE(errorsString.empty()) << errorsString;
    }
}