#pragma once

#include <oblo/core/handle.hpp>

#include <vulkan/vulkan.h>

#include <span>
#include <vector>

namespace oblo
{
    class string_interner;
    struct string;
}

namespace oblo::vk
{
    class allocator;
    class resource_manager;
    struct buffer;

    class buffer_table
    {
    public:
        struct column_description;

    public:
        buffer_table();
        buffer_table(const buffer_table&) = delete;
        buffer_table(buffer_table&&) noexcept = delete;
        buffer_table& operator=(const buffer_table&) = delete;
        buffer_table& operator=(buffer_table&&) noexcept = delete;
        ~buffer_table();

        void init(std::span<const column_description> columns,
                  allocator& allocator,
                  resource_manager& resourceManager,
                  VkBufferUsageFlags bufferUsage,
                  u32 rows);

        void shutdown(allocator& allocator, resource_manager& resourceManager);

        std::span<const handle<string>> names() const;
        std::span<const handle<buffer>> buffers() const;

        i32 try_find(handle<string> name) const;

    private:
        struct column;
        i32* m_stringToBufferIndexMap{nullptr};
        handle<buffer>* m_buffers{nullptr};
        handle<string>* m_names{nullptr};
        u32 m_rows{0u};
        u32 m_columns{0u};
        u32 m_stringRangeMin{0u};
        u32 m_stringRangeMax{0u};
    };

    struct buffer_table::column_description
    {
        handle<string> name;
        u32 elementSize;
    };
}