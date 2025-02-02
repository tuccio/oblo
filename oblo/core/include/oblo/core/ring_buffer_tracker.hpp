#pragma once

#include <oblo/core/debug.hpp>

#include <concepts>
#include <cstddef>

namespace oblo
{
    template <typename Size>
    class ring_buffer_tracker
    {
    public:
        struct segment
        {
            Size begin;
            Size end;
        };

        struct segmented_span
        {
            segment segments[2];
        };

    public:
        ring_buffer_tracker() = default;
        ring_buffer_tracker(const ring_buffer_tracker&) = default;
        ring_buffer_tracker(ring_buffer_tracker&& other) = default;

        explicit ring_buffer_tracker(Size size)
        {
            m_size = size;
        }

        ring_buffer_tracker& operator=(const ring_buffer_tracker&) = default;
        ring_buffer_tracker& operator=(ring_buffer_tracker&& other) noexcept = default;

        ~ring_buffer_tracker() = default;

        void reset(Size size = 0u)
        {
            m_size = size;
            m_firstUnused = 0u;
            m_usedCount = 0u;
        }

        bool has_available(Size count)
        {
            return count <= m_size - m_usedCount;
        }

        segmented_span fetch(Size count)
        {
            OBLO_ASSERT(has_available(count));

            const auto availableFirstSegment = m_size - m_firstUnused;

            segmented_span result{};

            if (count <= availableFirstSegment)
            {
                result.segments[0] = {.begin = m_firstUnused, .end = m_firstUnused + count};
                m_firstUnused = (m_firstUnused + count) % m_size;
            }
            else
            {
                const auto remaining = count - availableFirstSegment;
                result.segments[0] = {.begin = m_firstUnused, .end = m_firstUnused + availableFirstSegment};
                result.segments[1] = {.begin = 0u, .end = remaining};
                m_firstUnused = remaining;
            }

            m_usedCount += count;

            return result;
        }

        void release(Size count)
        {
            OBLO_ASSERT(count <= m_usedCount);
            m_usedCount -= count;
        }

        Size size() const
        {
            return m_size;
        }

        Size used_count() const
        {
            return m_usedCount;
        }

        Size available_count() const
        {
            return m_size - m_usedCount;
        }

        Size first_unused() const
        {
            return m_firstUnused;
        }

    private:
        Size m_size{0u};
        Size m_firstUnused{0u};
        Size m_usedCount{0u};
    };
}