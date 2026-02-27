#include <oblo/core/suballocation/buffer_table.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/iterator/zip_iterator.hpp>
#include <oblo/core/stack_allocator.hpp>
#include <oblo/math/power_of_two.hpp>

#include <algorithm>
#include <memory>

namespace oblo
{
    namespace
    {
        constexpr i32 Invalid{-1};
    }

    buffer_table::buffer_table() = default;

    buffer_table::~buffer_table()
    {
        shutdown();
    }

    expected<u64> buffer_table::init(u64 memStartOffset,
        u64 memTotalSize,
        std::span<const column_description> columns,
        u64 rows,
        u64 bufferAlignment)
    {
        if (!is_power_of_two(bufferAlignment))
        {
            return "Alignment has to be a power of two"_err;
        }

        if (columns.empty() || rows == 0u)
        {
            return 0u;
        }

        buffered_array<u64, 64> bufferOffsets;
        bufferOffsets.resize_default(columns.size());

        u64 currentOffset = memStartOffset;
        for (u32 j = 0; j < columns.size(); ++j)
        {
            currentOffset = align_power_of_two(currentOffset, bufferAlignment);
            bufferOffsets[j] = currentOffset;

            const auto columnSize = columns[j].elementSize * rows;
            currentOffset += columnSize;
        }

        if (currentOffset > memStartOffset + memTotalSize)
        {
            return 0u;
        }

        h32<buffer_table_name> min{~0u}, max{0u};

        for (const buffer_table_column_description& column : columns)
        {
            if (!column.name)
            {
                return "Invalid name"_err;
            }

            if (column.name < min)
            {
                min = column.name;
            }

            if (column.name > max)
            {
                max = column.name;
            }
        }

        m_columns = columns.size();
        m_stringRangeMin = min.value;
        m_stringRangeMax = max.value;

        const auto range = 1 + max.value - min.value;

        const auto bufferToIndexMapSize = range * sizeof(m_stringToBufferIndexMap[0]);
        const auto buffersSize = m_columns * (sizeof(m_bufferSubranges[0]));
        const auto namesSize = m_columns * (sizeof(m_names[0]));
        const auto elementSizes = m_columns * (sizeof(m_elementSizes[0]));

        auto* const heapBuffer = new std::byte[bufferToIndexMapSize + buffersSize + namesSize + elementSizes];

        m_stringToBufferIndexMap = reinterpret_cast<i32*>(heapBuffer);
        std::uninitialized_fill(m_stringToBufferIndexMap, m_stringToBufferIndexMap + range, Invalid);

        m_bufferSubranges = new (heapBuffer + bufferToIndexMapSize) buffer_table_subrange;
        m_names = new (heapBuffer + bufferToIndexMapSize + buffersSize) h32<buffer_table_name>;
        m_elementSizes = new (heapBuffer + bufferToIndexMapSize + buffersSize + namesSize) u64;

        for (u64 j = 0; j < m_columns; ++j)
        {
            const auto [name, elementSize] = columns[j];
            const u64 columnSize = elementSize * rows;

            m_names[j] = name;
            m_elementSizes[j] = elementSize;
            m_bufferSubranges[j] = {bufferOffsets[j], bufferOffsets[j] + columnSize};
        }

        m_rows = rows;

        const auto begin = zip_iterator{m_names, m_bufferSubranges, m_elementSizes};
        const auto end = begin + m_columns;

        std::sort(begin,
            end,
            [](const auto& lhs, const auto& rhs)
            {
                const h32<buffer_table_name> lhsName = std::get<0>(lhs);
                const h32<buffer_table_name> rhsName = std::get<0>(rhs);
                return lhsName < rhsName;
            });

        for (u32 j = 0; j < m_columns; ++j)
        {
            const auto name = m_names[j];
            m_stringToBufferIndexMap[name.value - min.value] = i32(j);
        }

        return currentOffset - memStartOffset;
    }

    void buffer_table::shutdown()
    {
        if (m_stringToBufferIndexMap)
        {
            delete[] reinterpret_cast<std::byte*>(m_stringToBufferIndexMap);

            m_stringToBufferIndexMap = nullptr;
            m_bufferSubranges = nullptr;
            m_names = nullptr;
            m_rows = 0u;
            m_columns = 0u;
        }
    }

    std::span<const h32<buffer_table_name>> buffer_table::names() const
    {
        return {m_names, m_columns};
    }

    std::span<const buffer_table_subrange> buffer_table::buffer_subranges() const
    {
        return {m_bufferSubranges, m_columns};
    }

    std::span<const u64> buffer_table::element_sizes() const
    {
        return {m_elementSizes, m_columns};
    }

    i32 buffer_table::find(h32<buffer_table_name> name) const
    {
        if (name.value < m_stringRangeMin || name.value > m_stringRangeMax)
        {
            return -1;
        }

        return m_stringToBufferIndexMap[name.value - m_stringRangeMin];
    }

    u64 buffer_table::rows_count() const
    {
        return m_rows;
    }

    u64 buffer_table::columns_count() const
    {
        return m_columns;
    }
}