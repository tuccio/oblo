#include <gtest/gtest.h>

#include <oblo/core/array_size.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/buffer_table.hpp>
#include <oblo/vulkan/resource_manager.hpp>

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

            allocator allocator;
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

            buffer_table bufferTable;
            bufferTable.init(columns, allocator, resourceManager, VK_BUFFER_USAGE_TRANSFER_DST_BIT, numRows);

            const std::span bufferColumns = bufferTable.buffers();
            const std::span nameColumns = bufferTable.names();
            const std::span elementSizes = bufferTable.element_sizes();

            ASSERT_EQ(numColumns, bufferColumns.size());
            ASSERT_EQ(numColumns, nameColumns.size());

            ASSERT_TRUE(std::is_sorted(std::begin(nameColumns), std::end(nameColumns)));

            for (const auto& column : columns)
            {
                const auto index = bufferTable.try_find(column.name);
                ASSERT_GE(index, 0);

                ASSERT_EQ(nameColumns[index], column.name);
                ASSERT_EQ(elementSizes[index], column.elementSize);
                ASSERT_TRUE(bufferColumns[index]);

                const auto buffer = resourceManager.get(bufferColumns[index]);
                ASSERT_EQ(buffer.size, numRows * column.elementSize);
            }

            bufferTable.shutdown(allocator, resourceManager);
        }

        const auto errorsString = validationErrors.str();
        ASSERT_TRUE(errorsString.empty()) << errorsString;
    }
}