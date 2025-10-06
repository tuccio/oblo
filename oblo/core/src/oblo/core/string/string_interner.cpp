#include <oblo/core/string/string_interner.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/string/transparent_string_hash.hpp>

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

        struct string_storage
        {
            cstring_view view;
            hash_type hash;
        };
    }

    struct string_interner::impl
    {
        std::unordered_map<hashed_string_view, u32, transparent_string_hash> sparse;
        dynamic_array<string_storage> strings;
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
        return get_or_add(hashed_string_view{str});
    }

    h32<string> string_interner::get_or_add(hashed_string_view str)
    {
        OBLO_ASSERT(str.size() <= MaxStringLength);
        auto& sparse = m_impl->sparse;
        const auto it = sparse.find(str);

        if (it == sparse.end())
        {
            const auto stringIndex = m_impl->strings.size32();

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

            const hashed_string_view sv{newStringPtr, stringLength};

            m_impl->strings.emplace_back(cstring_view{newStringPtr, stringLength}, sv.hash());
            m_impl->sparse.emplace(sv, stringIndex);

            return {stringIndex};
        }
        else
        {
            return {it->second};
        }
    }

    h32<string> string_interner::get(string_view str) const
    {
        return get(hashed_string_view{str});
    }

    h32<string> string_interner::get(hashed_string_view str) const
    {
        auto& sparse = m_impl->sparse;
        const auto it = sparse.find(str);
        return {it == sparse.end() ? 0u : it->second};
    }

    cstring_view string_interner::str(h32<string> handle) const
    {
        OBLO_ASSERT(handle && handle.value < m_impl->strings.size());
        return m_impl->strings[handle.value].view;
    }

    hashed_string_view string_interner::h_str(h32<string> handle) const
    {
        OBLO_ASSERT(handle && handle.value < m_impl->strings.size());
        const auto& str = m_impl->strings[handle.value];
        return hashed_string_view{str.view, str.hash};
    }

    const char* string_interner::c_str(h32<string> handle) const
    {
        OBLO_ASSERT(handle && handle.value < m_impl->strings.size());
        return m_impl->strings[handle.value].view.c_str();
    }
}