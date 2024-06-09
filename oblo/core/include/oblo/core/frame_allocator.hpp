#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    class frame_allocator
    {
    public:
        class scoped_restore;

        frame_allocator() = default;
        frame_allocator(const frame_allocator&) = delete;
        frame_allocator(frame_allocator&&) noexcept;
        frame_allocator& operator=(const frame_allocator&) = delete;
        frame_allocator& operator=(frame_allocator&&) noexcept = delete;
        ~frame_allocator();

        bool init(usize maxSize, usize chunkSize = 4u << 20, usize startingChunks = 0);
        void shutdown();

        void* allocate(usize size, usize alignment);
        void free_unused();

        void restore(void* point);
        void restore_all();

        usize get_committed_memory_size() const;

        void* get_first_unallocated_byte() const;

        scoped_restore make_scoped_restore();

        bool contains(const void* ptr) const;

    private:
        u8* m_virtualMemory{nullptr};
        u8* m_end{nullptr};
        u8* m_commitEnd{nullptr};
        usize m_chunkSize{0};
    };

    class frame_allocator::scoped_restore
    {
    public:
        explicit scoped_restore(frame_allocator* allocator, void* point) : m_allocator{allocator}, m_point{point} {}

        scoped_restore(const scoped_restore&) = delete;
        scoped_restore(scoped_restore&&) noexcept = delete;
        scoped_restore& operator=(const scoped_restore&) = delete;
        scoped_restore& operator=(scoped_restore&&) noexcept = delete;

        ~scoped_restore()
        {
            if (m_allocator && m_point)
            {
                m_allocator->restore(m_point);
            }
        }

    private:
        frame_allocator* m_allocator{nullptr};
        void* m_point{nullptr};
    };

    inline frame_allocator::scoped_restore frame_allocator::make_scoped_restore()
    {
        return scoped_restore{this, m_end};
    }
}