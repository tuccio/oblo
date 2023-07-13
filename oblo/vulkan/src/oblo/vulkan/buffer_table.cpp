#include <oblo/vulkan/buffer_table.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/zip_iterator.hpp>
#include <oblo/vulkan/allocator.hpp>
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

    void buffer_table::init(std::span<const column_description> columns,
                            allocator& allocator,
                            resource_manager& resourceManager,
                            VkBufferUsageFlags bufferUsage,
                            u32 rows)
    {
        if (columns.empty())
        {
            return;
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

        m_stringRangeMin = min.value;
        m_stringRangeMax = max.value;

        const auto range = 1 + max.value - min.value;

        m_columns = columns.size();

        const auto bufferToIndexMapSize = range * sizeof(m_stringToBufferIndexMap[0]);
        const auto buffersSize = m_columns * (sizeof(m_buffers[0]));
        const auto namesSize = m_columns * (sizeof(m_names[0]));

        auto* const heapBuffer = new std::byte[bufferToIndexMapSize + buffersSize + namesSize];

        m_stringToBufferIndexMap = reinterpret_cast<i32*>(heapBuffer);

        m_buffers = reinterpret_cast<h32<buffer>*>(heapBuffer + bufferToIndexMapSize);
        m_names = reinterpret_cast<h32<string>*>(heapBuffer + bufferToIndexMapSize + buffersSize);

        std::uninitialized_fill(m_stringToBufferIndexMap, m_stringToBufferIndexMap + range, Invalid);
        std::uninitialized_value_construct_n(m_buffers, m_columns);

        for (u32 j = 0; j < m_columns; ++j)
        {
            const auto name = columns[j].name;
            new (m_names + j) h32<string>{name};
        }

        m_rows = rows;

        if (rows > 0)
        {
            for (u32 j = 0; j < m_columns; ++j)
            {
                const auto size = u32(columns[j].elementSize * rows);

                allocated_buffer allocatedBuffer;

                OBLO_VK_PANIC(allocator.create_buffer(
                    {
                        .size = size,
                        .usage = bufferUsage,
                        .memoryUsage = memory_usage::gpu_only,
                    },
                    &allocatedBuffer));

                m_buffers[j] = resourceManager.register_buffer({
                    .buffer = allocatedBuffer.buffer,
                    .offset = 0u,
                    .size = size,
                    .allocation = allocatedBuffer.allocation,
                });
            }
        }

        const auto begin = zip_iterator{m_names, m_buffers};
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
    }

    void buffer_table::shutdown(allocator& allocator, resource_manager& resourceManager)
    {
        if (m_buffers)
        {
            for (u32 j = 0u; j < m_columns; ++j)
            {
                const auto handle = m_buffers[j];

                if (handle)
                {
                    const auto& buffer = resourceManager.get(handle);
                    allocator.destroy(allocated_buffer{.buffer = buffer.buffer, .allocation = buffer.allocation});
                    resourceManager.unregister_buffer(handle);
                }
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

    i32 buffer_table::try_find(h32<string> name) const
    {
        if (name.value < m_stringRangeMin || name.value > m_stringRangeMax)
        {
            return -1;
        }

        return m_stringToBufferIndexMap[name.value - m_stringRangeMin];
    }
}