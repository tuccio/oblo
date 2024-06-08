#include <oblo/vulkan/buffer_table.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/stack_allocator.hpp>
#include <oblo/core/iterator/zip_iterator.hpp>
#include <oblo/math/power_of_two.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/resource_manager.hpp>

#include <algorithm>
#include <memory>

namespace oblo::vk
{
    namespace
    {
        constexpr i32 Invalid{-1};
    }

    buffer_table::buffer_table() = default;

    buffer_table::~buffer_table()
    {
        OBLO_ASSERT(!m_buffers, "This instance needs to be shutdown explicitly");
    }

    u32 buffer_table::init(const buffer& buf,
        std::span<const column_description> columns,
        resource_manager& resourceManager,
        u32 rows,
        u32 bufferAlignment)
    {
        if (columns.empty() || rows == 0u || !is_power_of_two(bufferAlignment))
        {
            return 0u;
        }

        array_stack_allocator<u32, 64> stackAllocator;

        u32* const bufferOffsets = allocate_n<u32>(stackAllocator, columns.size());
        const auto numColumns = u32(columns.size());

        u32 currentOffset = buf.offset;
        for (u32 j = 0; j < numColumns; ++j)
        {
            currentOffset = align_power_of_two(currentOffset, bufferAlignment);
            bufferOffsets[j] = currentOffset;

            const auto columnSize = columns[j].elementSize * rows;
            currentOffset += columnSize;
        }

        if (currentOffset >= buf.offset + buf.size)
        {
            return 0u;
        }

        h32<string> min{~0u}, max{0u};

        for (const auto& column : columns)
        {
            if (column.name < min)
            {
                min = column.name;
            }

            if (column.name > max)
            {
                max = column.name;
            }
        }

        m_columns = numColumns;
        m_stringRangeMin = min.value;
        m_stringRangeMax = max.value;

        const auto range = 1 + max.value - min.value;

        const auto bufferToIndexMapSize = range * sizeof(m_stringToBufferIndexMap[0]);
        const auto buffersSize = m_columns * (sizeof(m_buffers[0]));
        const auto namesSize = m_columns * (sizeof(m_names[0]));
        const auto elementSizes = m_columns * (sizeof(m_elementSizes[0]));

        auto* const heapBuffer = new std::byte[bufferToIndexMapSize + buffersSize + namesSize + elementSizes];

        m_stringToBufferIndexMap = reinterpret_cast<i32*>(heapBuffer);

        m_buffers = reinterpret_cast<h32<buffer>*>(heapBuffer + bufferToIndexMapSize);
        m_names = reinterpret_cast<h32<string>*>(heapBuffer + bufferToIndexMapSize + buffersSize);
        m_elementSizes = reinterpret_cast<u32*>(heapBuffer + bufferToIndexMapSize + buffersSize + namesSize);

        std::uninitialized_fill(m_stringToBufferIndexMap, m_stringToBufferIndexMap + range, Invalid);

        for (u32 j = 0; j < m_columns; ++j)
        {
            const auto columnSize = columns[j].elementSize * rows;

            const buffer newBuffer{
                .buffer = buf.buffer,
                .offset = bufferOffsets[j],
                .size = columnSize,
                .allocation = buf.allocation,
            };

            const auto newBufferHandle = resourceManager.register_buffer(newBuffer);

            const auto [name, elementSize] = columns[j];
            new (m_names + j) h32<string>{name};
            new (m_elementSizes + j) u32{elementSize};
            new (m_buffers + j) h32<buffer>{newBufferHandle};
        }

        m_rows = rows;

        const auto begin = zip_iterator{m_names, m_buffers, m_elementSizes};
        const auto end = begin + m_columns;

        std::sort(begin,
            end,
            [](const auto& lhs, const auto& rhs)
            {
                const h32<string> lhsName = std::get<0>(lhs);
                const h32<string> rhsName = std::get<0>(rhs);
                return lhsName < rhsName;
            });

        for (u32 j = 0; j < m_columns; ++j)
        {
            const auto name = m_names[j];
            m_stringToBufferIndexMap[name.value - min.value] = i32(j);
        }

        return currentOffset - buf.offset;
    }

    void buffer_table::shutdown(resource_manager& resourceManager)
    {
        if (m_buffers)
        {
            for (u32 j = 0; j < m_columns; ++j)
            {
                resourceManager.unregister_buffer(m_buffers[j]);
            }

            delete[] reinterpret_cast<std::byte*>(m_stringToBufferIndexMap);

            m_stringToBufferIndexMap = nullptr;
            m_buffers = nullptr;
            m_names = nullptr;
            m_rows = 0u;
            m_columns = 0u;
        }
    }

    std::span<const h32<string>> buffer_table::names() const
    {
        return {m_names, m_columns};
    }

    std::span<const h32<buffer>> buffer_table::buffers() const
    {
        return {m_buffers, m_columns};
    }

    std::span<const u32> buffer_table::element_sizes() const
    {
        return {m_elementSizes, m_columns};
    }

    i32 buffer_table::find(h32<string> name) const
    {
        if (name.value < m_stringRangeMin || name.value > m_stringRangeMax)
        {
            return -1;
        }

        return m_stringToBufferIndexMap[name.value - m_stringRangeMin];
    }

    u32 buffer_table::rows_count() const
    {
        return m_rows;
    }

    u32 buffer_table::columns_count() const
    {
        return m_columns;
    }
}