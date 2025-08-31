#include <oblo/core/string/string_interner.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/hash.hpp>

#include <unordered_map>

namespace oblo
{
    namespace
    {
        struct string_chunk
        {
            static constexpr u16 Size{4096 - sizeof(void*)};
            char buf[Size];
            string_chunk* next;
        };
    }

    struct string_interner::impl
    {
        std::unordered_map<string_view, u32, hash<string_view>> sparse;
        dynamic_array<cstring_view> strings;
        string_chunk* chunks;
        u16 firstFree{string_chunk::Size};
    };

    string_interner::string_interner(string_interner&& other) noexcept
    {
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }

    string_interner::~string_interner()
    {
        shutdown();
    }

    string_interner& string_interner::operator=(string_interner&& other) noexcept
    {
        shutdown();
        m_impl = other.m_impl;
        other.m_impl = nullptr;
        return *this;
    }

    void string_interner::init(u32 estimatedStringsCount)
    {
        shutdown();

        OBLO_ASSERT(!m_impl);
        m_impl = new impl{};
        m_impl->sparse.reserve(estimatedStringsCount);
        m_impl->strings.reserve(estimatedStringsCount);
        m_impl->chunks = nullptr;

        // This deals with handle 0 being invalid (it will return an empty string)
        m_impl->strings.emplace_back();
    }

    void string_interner::shutdown()
    {
        if (m_impl)
        {
            for (auto* chunk = m_impl->chunks; chunk != nullptr;)
            {
                auto* next = chunk->next;
                delete chunk;
                chunk = next;
            }

            delete m_impl;
            m_impl = nullptr;
        }
    }

    h32<string> string_interner::get_or_add(string_view str)
    {
        OBLO_ASSERT(str.size() <= MaxStringLength);
        auto& sparse = m_impl->sparse;
        const auto it = sparse.find(str);

        if (it == sparse.end())
        {
            auto& strings = m_impl->strings;
            const auto stringIndex = u32(strings.size());

            const auto stringLength = str.size();
            const auto storageLength = stringLength + 1;

            char* newStringPtr;

            const auto oldFirstFree = m_impl->firstFree;
            const auto newFirstFree = oldFirstFree + storageLength;

            if (newFirstFree > string_chunk::Size)
            {
                auto* const newChunk = new string_chunk;
                newChunk->next = m_impl->chunks;
                m_impl->chunks = newChunk;
                m_impl->firstFree = u16(storageLength);
                newStringPtr = m_impl->chunks->buf;
            }
            else
            {
                newStringPtr = m_impl->chunks->buf + oldFirstFree;
                m_impl->firstFree = u16(newFirstFree);
            }

            std::memcpy(newStringPtr, str.data(), stringLength);
            newStringPtr[stringLength] = '\0';

            strings.emplace_back(newStringPtr, stringLength);
            m_impl->sparse.emplace(string_view{newStringPtr, stringLength}, stringIndex);

            return {stringIndex};
        }
        else
        {
            return {it->second};
        }
    }

    h32<string> string_interner::get(string_view str) const
    {
        auto& sparse = m_impl->sparse;
        const auto it = sparse.find(str);
        return {it == sparse.end() ? 0u : it->second};
    }

    cstring_view string_interner::str(h32<string> handle) const
    {
        OBLO_ASSERT(handle && handle.value < m_impl->strings.size());
        return m_impl->strings[handle.value];
    }

    const char* string_interner::c_str(h32<string> handle) const
    {
        OBLO_ASSERT(handle && handle.value < m_impl->strings.size());
        return m_impl->strings[handle.value].data();
    }
}