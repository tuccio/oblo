#include <gtest/gtest.h>

#include <oblo/core/suballocation/buffer_table.hpp>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_interner.hpp>

namespace oblo
{
    namespace
    {
        struct position
        {
            float x, y, z;
        };

        struct normal
        {
            float x, y, z;
        };

        struct tex_coord
        {
            float u, v;
        };
    }

    TEST(buffer_table, buffer_table)
    {
        string_interner interner;
        interner.init(8);

        const h32 posName = interner.get_or_add("position"_hsv);
        const h32 nrmName = interner.get_or_add("normal"_hsv);
        const h32 uvName = interner.get_or_add("tex_coord"_hsv);

        buffer_table table;

        constexpr u64 startOffset{0};
        constexpr u64 totalSize{1024};
        constexpr u64 numRows{10};
        constexpr u64 alignment{16};

        const buffer_table::column_description columns[] = {
            {.name = posName.rebind<buffer_table_name>(), .elementSize = sizeof(position)},
            {.name = nrmName.rebind<buffer_table_name>(), .elementSize = sizeof(normal)},
            {.name = uvName.rebind<buffer_table_name>(), .elementSize = sizeof(tex_coord)},
        };

        const auto expectedTotalSize = table.init(startOffset, totalSize, columns, numRows, alignment);
        ASSERT_TRUE(expectedTotalSize);
        ASSERT_LE(*expectedTotalSize, totalSize);

        ASSERT_EQ(table.rows_count(), numRows);
        ASSERT_EQ(table.columns_count(), 3);

        const auto subranges = table.buffer_subranges();
        ASSERT_EQ(subranges.size(), 3);

        const auto elementSizes = table.element_sizes();
        ASSERT_EQ(elementSizes.size(), 3);

        for (u32 i = 0; i < subranges.size(); ++i)
        {
            const auto& range = subranges[i];
            const auto elementSize = elementSizes[i];

            // Check alignment
            ASSERT_EQ(range.begin % alignment, 0);

            // Check bounds
            ASSERT_LE(range.end, totalSize);
            ASSERT_GE(range.begin, startOffset);

            // Check size
            ASSERT_GE(range.end - range.begin, elementSize * numRows);

            // Check for overlaps with other ranges
            for (u32 j = i + 1; j < subranges.size(); ++j)
            {
                const auto& otherRange = subranges[j];
                ASSERT_TRUE(range.end <= otherRange.begin || range.begin >= otherRange.end);
            }
        }

        ASSERT_NE(table.find(posName.rebind<buffer_table_name>()), -1);
        ASSERT_NE(table.find(nrmName.rebind<buffer_table_name>()), -1);
        ASSERT_NE(table.find(uvName.rebind<buffer_table_name>()), -1);

        struct unknown;
        const auto unknownName = interner.get_or_add("unknown"_hsv);
        ASSERT_EQ(table.find(unknownName.rebind<buffer_table_name>()), -1);
    }
}
