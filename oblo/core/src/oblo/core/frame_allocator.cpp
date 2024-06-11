#include <oblo/core/frame_allocator.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/utility.hpp>

#include <bit>

namespace oblo
{
    namespace
    {
        // Could consider reading the actual page size through system calls
        constexpr auto PageSize{16u << 10};

        void* virtual_memory_allocate(usize size);
        void virtual_memory_free(void* ptr);
        bool virtual_memory_commit(void* ptr, usize size);
        bool virtual_memory_decommit(void* ptr, usize size);
    }

    frame_allocator::frame_allocator(frame_allocator&& other) noexcept
    {
        std::swap(m_virtualMemory, other.m_virtualMemory);
        std::swap(m_end, other.m_end);
        std::swap(m_commitEnd, other.m_commitEnd);
        std::swap(m_chunkSize, other.m_chunkSize);
    }

    frame_allocator::~frame_allocator()
    {
        shutdown();
    }

    bool frame_allocator::init(usize maxSize, usize chunkSize, usize startingChunks)
    {
        if (m_virtualMemory)
        {
            return false;
        }

        if (chunkSize % PageSize != 0)
        {
            return false;
        }

        if (maxSize < chunkSize * startingChunks)
        {
            return false;
        }

        m_virtualMemory = static_cast<u8*>(virtual_memory_allocate(maxSize));
        m_end = m_virtualMemory;

        if (const auto initialCommit = chunkSize * startingChunks;
            m_virtualMemory && startingChunks > 0 && virtual_memory_commit(m_virtualMemory, initialCommit))
        {
            m_commitEnd = m_virtualMemory + initialCommit;
        }
        else
        {
            m_commitEnd = m_virtualMemory;
        }

        m_chunkSize = chunkSize;

        return m_virtualMemory != nullptr;
    }

    void frame_allocator::shutdown()
    {
        if (m_virtualMemory)
        {
            virtual_memory_free(m_virtualMemory);
            m_virtualMemory = nullptr;
        }
    }

    byte* frame_allocator::allocate(usize size, usize alignment) noexcept
    {
        OBLO_ASSERT(size != 0);
        OBLO_ASSERT(alignment != 0);

        const auto misalignment = std::bit_cast<uintptr>(m_end) % alignment;
        const auto alignmentOffset = (alignment - misalignment) % alignment;

        auto* const ptr = m_end + alignmentOffset;
        const auto newEnd = ptr + size;

        if (newEnd > m_commitEnd)
        {
            const auto prevCommittedSize = get_committed_memory_size();
            OBLO_ASSERT(prevCommittedSize % m_chunkSize == 0);
            const auto prevChunksCount = prevCommittedSize / m_chunkSize;

            const auto newBytesSize = usize(newEnd - m_virtualMemory);
            const auto newChunksCount = round_up_div(newBytesSize, m_chunkSize);

            OBLO_ASSERT(newChunksCount > prevChunksCount);

            const auto bytesToCommit = (newChunksCount - prevChunksCount) * m_chunkSize;

            if (virtual_memory_commit(m_commitEnd, bytesToCommit))
            {
                m_commitEnd += bytesToCommit;
            }
            else
            {
                return nullptr;
            }
        }

        m_end = newEnd;
        return reinterpret_cast<byte*>(ptr);
    }

    void frame_allocator::free_unused()
    {
        const auto prevCommittedSize = get_committed_memory_size();
        OBLO_ASSERT(prevCommittedSize % prevCommittedSize == 0);
        const auto prevChunksCount = prevCommittedSize / m_chunkSize;

        const auto newBytesSize = usize(m_end - m_virtualMemory);
        const auto newChunksCount = round_up_div(newBytesSize, m_chunkSize);
        OBLO_ASSERT(newChunksCount <= prevChunksCount);

        if (newChunksCount < prevChunksCount)
        {
            const auto bytesToDecommit = (prevChunksCount - newChunksCount) * m_chunkSize;

            if (const auto newEnd = m_commitEnd - bytesToDecommit; virtual_memory_decommit(newEnd, bytesToDecommit))
            {
                m_commitEnd = newEnd;
            }
        }
    }

    void frame_allocator::restore(void* savePoint)
    {
        OBLO_ASSERT(savePoint <= m_end);
        m_end = static_cast<u8*>(savePoint);
    }

    void frame_allocator::restore_all()
    {
        m_end = m_virtualMemory;
    }

    usize frame_allocator::get_committed_memory_size() const
    {
        return usize(m_commitEnd - m_virtualMemory);
    }

    void* frame_allocator::get_first_unallocated_byte() const
    {
        return m_end;
    }

    bool frame_allocator::contains(const void* ptr) const
    {
        return ptr >= m_virtualMemory && ptr < m_commitEnd;
    }
}

#ifdef WIN32

#include <Windows.h>

namespace oblo
{
    namespace
    {
        void* virtual_memory_allocate(usize size)
        {
            return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
        }

        void virtual_memory_free(void* ptr)
        {
            [[maybe_unused]] const auto success = VirtualFree(ptr, 0, MEM_RELEASE);
            OBLO_ASSERT(success);
        }

        bool virtual_memory_commit(void* ptr, usize size)
        {
            return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
        }

        bool virtual_memory_decommit(void* ptr, usize size)
        {
            return VirtualFree(ptr, size, MEM_DECOMMIT) != FALSE;
        }
    }
}

#endif