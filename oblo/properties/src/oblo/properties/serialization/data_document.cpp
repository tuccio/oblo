#include <oblo/properties/serialization/data_document.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_node.hpp>

#include <memory>

namespace oblo
{
    namespace
    {
        constexpr decltype(data_node::objectOrArray) make_invalid_object_or_array()
        {
            return {
                .firstChild = data_node::Invalid,
                .lastChild = data_node::Invalid,
            };
        }
    }

    struct data_document::data_chunk
    {
        data_chunk* next;
        u32 size;
        u32 used;
    };

    data_document::data_document() = default;

    data_document::data_document(data_document&& other) noexcept :
        m_nodes{std::move(other.m_nodes)}, m_firstChunk{other.m_firstChunk}, m_currentChunk{other.m_currentChunk},
        m_firstChunkSize{other.m_firstChunkSize}, m_chunksCount{other.m_chunksCount}
    {
        other.m_firstChunk = nullptr;
        other.m_currentChunk = nullptr;
        other.m_firstChunkSize = 0;
        other.m_chunksCount = 0;
    }

    data_document::~data_document()
    {
        for (auto* chunk = m_firstChunk; chunk != nullptr;)
        {
            auto* const next = chunk = chunk->next;
            delete chunk;
            chunk = next;
        }
    }

    data_document& data_document::operator=(data_document&& other) noexcept
    {
        this->~data_document();
        new (this) data_document(std::move(other));
        return *this;
    }

    void data_document::init(u32 firstChunkSize)
    {
        m_firstChunkSize = firstChunkSize;

        if (!m_firstChunk)
        {
            m_firstChunk = allocate_chunk(1);
            m_chunksCount = 1;
        }
        else
        {
            for (auto* chunk = m_firstChunk; chunk != nullptr; chunk = chunk->next)
            {
                chunk->used = 0;
            }
        }

        m_currentChunk = m_firstChunk;

        const data_node root{
            .kind = data_node_kind::object,
            .nextSibling = data_node::Invalid,
            .objectOrArray = make_invalid_object_or_array(),
        };

        m_nodes.assign(1, root);
    }

    bool data_document::is_initialized() const
    {
        return !m_nodes.empty();
    }

    bool data_document::is_object(u32 nodeIndex) const
    {
        return nodeIndex != data_node::Invalid && m_nodes[nodeIndex].kind == data_node_kind::object;
    }

    bool data_document::is_array(u32 nodeIndex) const
    {
        return nodeIndex != data_node::Invalid && m_nodes[nodeIndex].kind == data_node_kind::array;
    }

    bool data_document::is_value(u32 nodeIndex) const
    {
        return nodeIndex != data_node::Invalid && m_nodes[nodeIndex].kind == data_node_kind::value;
    }

    u32 data_document::get_root() const
    {
        if (m_nodes.empty())
        {
            return data_node::Invalid;
        }

        return 0u;
    }

    u32 data_document::child_object(u32 parentIndex, hashed_string_view key)
    {
        const u32 newChild = u32(m_nodes.size());
        auto& newObject = m_nodes.emplace_back();

        auto& parent = m_nodes[parentIndex];
        OBLO_ASSERT(parent.kind == data_node_kind::object || parent.kind == data_node_kind::array);

        const char* newKey{};
        u16 keyLen{};

        if (parent.kind == data_node_kind::object)
        {
            newKey = allocate_key(key);
            keyLen = narrow_cast<u16>(key.size());
        }

        newObject = {
            .kind = data_node_kind::object,
            .keyLen = keyLen,
            .nextSibling = data_node::Invalid,
            .key = newKey,
            .keyHash = key.hash(),
            .objectOrArray = make_invalid_object_or_array(),
        };

        append_new_child(parent, newChild);

        return newChild;
    }

    void data_document::child_value(
        u32 parentIndex, hashed_string_view key, property_kind kind, std::span<const byte> data)
    {
        const u32 newChild = u32(m_nodes.size());
        auto& newValue = m_nodes.emplace_back();

        auto& parent = m_nodes[parentIndex];
        OBLO_ASSERT(parent.kind == data_node_kind::object || parent.kind == data_node_kind::array);

        make_value(newChild, kind, data);

        if (parent.kind == data_node_kind::object)
        {
            newValue.key = allocate_key(key);
            newValue.keyLen = narrow_cast<u16>(key.size());
            newValue.keyHash = key.hash();
        }

        newValue.nextSibling = data_node::Invalid;

        append_new_child(parent, newChild);
    }

    u32 data_document::child_array(u32 parentIndex, hashed_string_view key, u32 size)
    {
        const u32 newArrayIndex = u32(m_nodes.size());

        // Create the array and all the elements (default initialized to kind none)
        m_nodes.resize(m_nodes.size() + 1 + size);

        auto& newArray = m_nodes[newArrayIndex];

        auto& parent = m_nodes[parentIndex];
        OBLO_ASSERT(parent.kind == data_node_kind::object || parent.kind == data_node_kind::array);

        newArray = {
            .kind = data_node_kind::array,
            .keyLen = narrow_cast<u16>(key.size()),
            .nextSibling = data_node::Invalid,
            .key = allocate_key(key),
            .keyHash = key.hash(),
            .objectOrArray =
                {
                    .firstChild = data_node::Invalid,
                    .lastChild = data_node::Invalid,
                    .childrenCount = 0,
                },
        };

        append_new_child(parent, newArrayIndex);

        for (u32 i = 0; i < size; ++i)
        {
            append_new_child(newArray, newArrayIndex + 1 + i);
        }

        if (size > 0)
        {
            m_nodes.back().nextSibling = data_node::Invalid;
        }

        return newArrayIndex;
    }

    u32 data_document::array_push_back(u32 arrayIndex)
    {
        auto& array = m_nodes[arrayIndex];
        OBLO_ASSERT(array.kind == data_node_kind::array);

        const u32 newArrayElement = u32(m_nodes.size());
        auto& e = m_nodes.emplace_back();
        e.nextSibling = data_node::Invalid;

        append_new_child(array, newArrayElement);

        return newArrayElement;
    }

    u32 data_document::child_next(u32 objectOrArray, u32 previous) const
    {
        auto& node = m_nodes[objectOrArray];
        OBLO_ASSERT(node.kind == data_node_kind::array || node.kind == data_node_kind::object);

        if (node.kind != data_node_kind::array && node.kind != data_node_kind::object)
        {
            return data_node::Invalid;
        }

        if (previous == data_node::Invalid)
        {
            return node.objectOrArray.firstChild;
        }

        return m_nodes[previous].nextSibling;
    }

    u32 data_document::children_count(u32 objectOrArray) const
    {
        auto& node = m_nodes[objectOrArray];
        OBLO_ASSERT(node.kind == data_node_kind::object || node.kind == data_node_kind::array);

        return node.objectOrArray.childrenCount;
    }

    void data_document::make_array(u32 node)
    {
        auto& n = m_nodes[node];
        n.kind = data_node_kind::array;
        n.objectOrArray = make_invalid_object_or_array();
    }

    void data_document::make_object(u32 node)
    {
        auto& n = m_nodes[node];
        n.kind = data_node_kind::object;
        n.objectOrArray = make_invalid_object_or_array();
    }

    void data_document::make_value(u32 nodeIndex, property_kind kind, std::span<const byte> data)
    {
        auto& newValue = m_nodes[nodeIndex];

        const auto [size, alignment] = get_size_and_alignment(kind);
        OBLO_ASSERT(data.size() >= size);

        void* newData{};

        if (size != 0)
        {
            newData = allocate(size, alignment);
            std::memcpy(newData, data.data(), size);
        }
        else if (kind == property_kind::string)
        {
            const auto dataString = *reinterpret_cast<const data_string*>(data.data());

            auto* const valueStr = allocate_key({dataString.data, dataString.length});

            newData = allocate(sizeof(data_string), alignof(data_string));
            new (newData) data_string{.data = valueStr, .length = dataString.length};
        }

        newValue.kind = data_node_kind::value;
        newValue.valueKind = kind;
        newValue.value = {.data = newData};
    }

    void* data_document::allocate(usize size, usize alignment)
    {
        usize space = m_currentChunk->size - m_currentChunk->used;

        u8* const begin = reinterpret_cast<u8*>(m_currentChunk + 1);
        void* firstUnused = begin + m_currentChunk->used;

        void* const ptr = std::align(alignment, size, firstUnused, space);

        if (!ptr)
        {
            auto* newChunk = allocate_chunk(m_chunksCount + 1);
            m_currentChunk->next = newChunk;
            m_currentChunk = newChunk;
            ++m_chunksCount;

            return allocate(size, alignment);
        }
        else
        {
            m_currentChunk->used = u32(u32(static_cast<u8*>(ptr) - begin) + size);
        }

        return ptr;
    }

    data_document::data_chunk* data_document::allocate_chunk(u8 exponent)
    {
        const auto memorySize = m_firstChunkSize << (exponent - 1);
        auto* const memory = new u8[memorySize];
        return new (memory) data_chunk{
            .next = nullptr,
            .size = u32(memorySize - sizeof(data_chunk)),
            .used = 0,
        };
    }

    const char* data_document::allocate_key(string_view key)
    {
        auto* const newKey = static_cast<char*>(allocate(key.size() + 1, 1));
        std::memcpy(newKey, key.data(), key.size());
        newKey[key.size()] = '\0';
        return newKey;
    }

    void data_document::append_new_child(data_node& parent, u32 newChild)
    {
        if (parent.objectOrArray.firstChild == data_node::Invalid)
        {
            parent.objectOrArray.firstChild = newChild;
        }

        if (parent.objectOrArray.lastChild != data_node::Invalid)
        {
            m_nodes[parent.objectOrArray.lastChild].nextSibling = newChild;
        }

        parent.objectOrArray.lastChild = newChild;
        ++parent.objectOrArray.childrenCount;
    }

    const deque<data_node>& data_document::get_nodes() const
    {
        return m_nodes;
    }

    u32 data_document::find_child(u32 parent, hashed_string_view name) const
    {
        if (parent == data_node::Invalid)
        {
            return data_node::Invalid;
        }

        if (m_nodes[parent].kind != data_node_kind::object)
        {
            return data_node::Invalid;
        }

        for (u32 index = m_nodes[parent].objectOrArray.firstChild; index != data_node::Invalid;
             index = m_nodes[index].nextSibling)
        {
            if (get_node_name(index) == name)
            {
                return index;
            }
        }

        return data_node::Invalid;
    }

    hashed_string_view data_document::get_node_name(u32 node) const
    {
        auto& n = m_nodes[node];
        return hashed_string_view{string_view{n.key, n.keyLen}, n.keyHash};
    }

    expected<data_string, data_document::error> data_document::read_string(u32 node) const
    {
        auto& n = m_nodes[node];

        if (n.kind != data_node_kind::value)
        {
            return error::node_kind_mismatch;
        }

        if (n.valueKind == property_kind::string)
        {
            return *reinterpret_cast<const data_string*>(n.value.data);
        }

        return error::value_kind_mismatch;
    }

    expected<bool, data_document::error> data_document::read_bool(u32 node) const
    {
        auto& n = m_nodes[node];

        if (n.kind != data_node_kind::value)
        {
            return error::node_kind_mismatch;
        }

        if (n.valueKind == property_kind::boolean)
        {
            return *reinterpret_cast<const bool*>(n.value.data);
        }

        return error::value_kind_mismatch;
    }

    expected<f32, data_document::error> data_document::read_f32(u32 node) const
    {
        auto& n = m_nodes[node];

        if (n.kind != data_node_kind::value)
        {
            return error::node_kind_mismatch;
        }

        if (n.valueKind == property_kind::f32)
        {
            return *reinterpret_cast<const f32*>(n.value.data);
        }

        if (n.valueKind == property_kind::f64)
        {
            return f32(*reinterpret_cast<const f64*>(n.value.data));
        }

        return error::value_kind_mismatch;
    }

    expected<u32, data_document::error> data_document::read_u32(u32 node) const
    {
        auto& n = m_nodes[node];

        if (n.kind != data_node_kind::value)
        {
            return error::node_kind_mismatch;
        }

        switch (n.valueKind)
        {
        case property_kind::u8:
            return *reinterpret_cast<const u8*>(n.value.data);

        case property_kind::u16:
            return *reinterpret_cast<const u16*>(n.value.data);

        case property_kind::u32:
            return *reinterpret_cast<const u32*>(n.value.data);

        default:
            break;
        }

        return error::value_kind_mismatch;
    }
}