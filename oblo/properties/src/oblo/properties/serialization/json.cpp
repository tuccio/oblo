#include <oblo/properties/serialization/json.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/log/log.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/data_node.hpp>

#define RAPIDJSON_ASSERT(x) OBLO_ASSERT(x)

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>

namespace oblo::json
{
    namespace
    {
        struct Handler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, Handler>
        {
            enum class state
            {
                object_or_array_start,
                name_or_object_end,
                value_or_array_end,
                finished,
            };

            struct stack_node
            {
                u32 id;
                data_node_kind kind;
            };

            data_document& m_doc;
            state m_state{state::object_or_array_start};
            std::string m_lastString;

            deque<stack_node> m_stack;

            explicit Handler(data_document& doc) : m_doc{doc} {}

            bool StartObject()
            {
                if (m_stack.empty())
                {
                    const auto root = m_doc.get_root();
                    m_doc.make_object(root);
                    m_stack.assign(1, {root, data_node_kind::object});

                    m_state = state::name_or_object_end;
                    return true;
                }

                switch (m_state)
                {
                case state::object_or_array_start: {
                    m_state = state::name_or_object_end;

                    auto& parent = m_stack.back();
                    const auto newNode = m_doc.child_object(parent.id, hashed_string_view{m_lastString});
                    m_stack.push_back({newNode, data_node_kind::object});
                }
                    return true;

                case state::value_or_array_end: {

                    auto& parent = m_stack.back();

                    u32 newNode;

                    switch (parent.kind)
                    {
                    case data_node_kind::array:
                        newNode = m_doc.array_push_back(parent.id);
                        m_doc.make_object(newNode);
                        break;
                    case data_node_kind::object:
                        newNode = m_doc.child_object(parent.id, hashed_string_view{m_lastString});
                        break;
                    default:
                        unreachable();
                    }

                    m_stack.push_back({newNode, data_node_kind::object});
                    m_state = state::name_or_object_end;
                    m_lastString.clear();
                }
                    return true;

                default:
                    return failure();
                }
            }

            bool EndObject(rapidjson::SizeType)
            {
                if (m_state != state::name_or_object_end)
                {
                    return failure();
                }

                if (m_stack.empty())
                {
                    return failure();
                }

                m_stack.pop_back();
                m_state = next_state_from_stack();

                return true;
            }

            bool StartArray()
            {
                if (m_stack.empty())
                {
                    const auto root = m_doc.get_root();
                    m_doc.make_array(root);
                    m_stack.assign(1, {root, data_node_kind::array});
                    return true;
                }

                switch (m_state)
                {
                case state::object_or_array_start:
                    m_state = state::value_or_array_end;
                    return true;

                case state::value_or_array_end: {

                    auto& parent = m_stack.back();

                    u32 newNode;

                    switch (parent.kind)
                    {
                    case data_node_kind::array:
                        newNode = m_doc.array_push_back(parent.id);
                        m_doc.make_array(newNode);
                        break;
                    case data_node_kind::object:
                        newNode = m_doc.child_array(parent.id, hashed_string_view{m_lastString});
                        break;
                    default:
                        unreachable();
                    }

                    m_stack.push_back({newNode, data_node_kind::array});
                    m_state = state::value_or_array_end;
                    m_lastString.clear();
                }
                    return true;

                default:
                    return failure();
                }
            }

            bool EndArray(rapidjson::SizeType)
            {
                if (m_state != state::value_or_array_end)
                {
                    return failure();
                }

                if (m_stack.empty())
                {
                    return failure();
                }

                m_stack.pop_back();
                m_state = next_state_from_stack();

                return true;
            }

            bool String(const char* str, rapidjson::SizeType length, bool)
            {
                switch (m_state)
                {
                case state::name_or_object_end:
                    m_lastString.assign(str, length);
                    m_state = state::value_or_array_end;
                    return true;
                case state::value_or_array_end: {
                    const data_string sv{str, length};

                    m_doc.child_value(m_stack.back().id,
                        hashed_string_view{m_lastString},
                        property_kind::string,
                        std::as_bytes(std::span{&sv, 1}));

                    m_lastString.clear();
                    m_state = next_state_from_stack();
                }

                    return true;
                default:
                    return failure();
                }
            }

            bool Bool(bool value)
            {
                switch (m_state)
                {
                case state::value_or_array_end:
                    m_doc.child_value(m_stack.back().id,
                        hashed_string_view{m_lastString},
                        property_kind::boolean,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = next_state_from_stack();
                    return true;
                default:
                    return failure();
                }
            }

            bool Int(i32 value)
            {
                switch (m_state)
                {
                case state::value_or_array_end:
                    m_doc.child_value(m_stack.back().id,
                        hashed_string_view{m_lastString},
                        property_kind::i32,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = next_state_from_stack();
                    return true;
                default:
                    return failure();
                }
            }

            bool Uint(u32 value)
            {
                switch (m_state)
                {
                case state::value_or_array_end:
                    m_doc.child_value(m_stack.back().id,
                        hashed_string_view{m_lastString},
                        property_kind::u32,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = next_state_from_stack();
                    return true;
                default:
                    return failure();
                }
            }

            bool Int64(i64 value)
            {
                switch (m_state)
                {
                case state::value_or_array_end:
                    m_doc.child_value(m_stack.back().id,
                        hashed_string_view{m_lastString},
                        property_kind::i64,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = next_state_from_stack();
                    return true;
                default:
                    return failure();
                }
            }

            bool UiInt64(u64 value)
            {
                switch (m_state)
                {
                case state::value_or_array_end:
                    m_doc.child_value(m_stack.back().id,
                        hashed_string_view{m_lastString},
                        property_kind::u64,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = next_state_from_stack();
                    return true;
                default:
                    return failure();
                }
            }

            bool Double(f64 value)
            {
                switch (m_state)
                {
                case state::value_or_array_end:
                    m_doc.child_value(m_stack.back().id,
                        hashed_string_view{m_lastString},
                        property_kind::f64,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = next_state_from_stack();
                    return true;
                default:
                    return failure();
                }
            }

            bool Default()
            {
                OBLO_ASSERT(false);
                return false;
            }

            bool failure()
            {
                OBLO_ASSERT(false);
                return false;
            }

        private:
            state next_state_from_stack() const
            {
                if (m_stack.empty())
                {
                    return state::finished;
                }

                auto& parent = m_stack.back();

                switch (parent.kind)
                {
                case data_node_kind::array:
                    return state::value_or_array_end;
                case data_node_kind::object:
                    return state::name_or_object_end;
                default:
                    unreachable();
                }
            }
        };
    }

    expected<> read(data_document& doc, cstring_view source)
    {
        const auto file = filesystem::file_ptr{filesystem::open_file(source, "r")};

        if (!file)
        {
            return unspecified_error;
        }

        constexpr auto bufferSize{1024};
        char buffer[bufferSize];

        rapidjson::FileReadStream rs{file.get(), buffer, bufferSize};

        rapidjson::Reader reader{};

        doc.init();

        reader.IterativeParseInit();

        Handler handler{doc};

        while (!reader.IterativeParseComplete())
        {
            if (!reader.IterativeParseNext<rapidjson::kParseDefaultFlags>(rs, handler))
            {
                if (reader.HasParseError())
                {
                    log::debug("JSON Parse error {} at {}:{}",
                        i32(reader.GetParseErrorCode()),
                        source,
                        reader.GetErrorOffset());
                }

                return unspecified_error_tag{};
            }
        }

        if (reader.HasParseError())
        {
            return unspecified_error_tag{};
        }

        return success_tag{};
    }

    expected<> write(const data_document& doc, cstring_view destination)
    {
        const u32 root = doc.get_root();

        if (root == data_node::Invalid)
        {
            return unspecified_error_tag{};
        }

        const auto file = filesystem::file_ptr{filesystem::open_file(destination, "w")};

        if (!file)
        {
            return unspecified_error_tag{};
        }

        constexpr auto bufferSize{1024};
        char buffer[bufferSize];

        rapidjson::FileWriteStream ws{file.get(), buffer, bufferSize};
        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer{ws};

        const auto& nodes = doc.get_nodes();

        enum class parent_kind : u8
        {
            none,
            object,
            array,
        };

        struct stack_frame
        {
            u32 node;
            parent_kind closeParent;
        };

        deque<stack_frame> stack;

        stack.push_back({
            .node = doc.get_root(),
            .closeParent = parent_kind::none,
        });

        while (!stack.empty())
        {
            const auto [node, closeParent] = stack.back();
            stack.pop_back();

            switch (closeParent)
            {
            case parent_kind::object:
                writer.EndObject();
                break;

            case parent_kind::array:
                writer.EndArray();
                break;

            case parent_kind::none:
                break;
            }

            if (node == data_node::Invalid)
            {
                continue;
            }

            const auto& current = nodes[node];

            // We want to visit the sibling after the children, so we push it first
            if (current.nextSibling != data_node::Invalid)
            {
                stack.push_back({
                    .node = current.nextSibling,
                    .closeParent = parent_kind::none,
                });
            }

            switch (current.kind)
            {
            case data_node_kind::object:
                if (current.keyLen != 0)
                {
                    writer.String(current.key, current.keyLen);
                }

                if (writer.StartObject())
                {
                    stack.push_back({
                        .node = data_node::Invalid,
                        .closeParent = parent_kind::object,
                    });

                    stack.push_back({
                        .node = current.objectOrArray.firstChild,
                        .closeParent = parent_kind::none,
                    });
                }

                break;

            case data_node_kind::array:
                if (current.keyLen != 0)
                {
                    writer.String(current.key, current.keyLen);
                }

                if (writer.StartArray())
                {
                    stack.push_back({
                        .node = data_node::Invalid,
                        .closeParent = parent_kind::array,
                    });

                    stack.push_back({
                        .node = current.objectOrArray.firstChild,
                        .closeParent = parent_kind::none,
                    });
                }

                break;

            case data_node_kind::value:
                if (current.keyLen != 0)
                {
                    writer.String(current.key, current.keyLen);
                }

                switch (current.valueKind)
                {
                case property_kind::boolean:
                    writer.Bool(*static_cast<bool*>(current.value.data));
                    break;

                case property_kind::i8:
                    writer.Int(*static_cast<i8*>(current.value.data));
                    break;

                case property_kind::i16:
                    writer.Int(*static_cast<i16*>(current.value.data));
                    break;

                case property_kind::i32:
                    writer.Int(*static_cast<i32*>(current.value.data));
                    break;

                case property_kind::i64:
                    writer.Int64(*static_cast<i64*>(current.value.data));
                    break;

                case property_kind::u8:
                    writer.Uint(*static_cast<u8*>(current.value.data));
                    break;

                case property_kind::u16:
                    writer.Uint(*static_cast<u16*>(current.value.data));
                    break;

                case property_kind::u32:
                    writer.Uint(*static_cast<u32*>(current.value.data));
                    break;

                case property_kind::u64:
                    writer.Uint64(*static_cast<u64*>(current.value.data));
                    break;

                case property_kind::f32:
                    writer.Double(*static_cast<f32*>(current.value.data));
                    break;

                case property_kind::f64:
                    writer.Double(*static_cast<f64*>(current.value.data));
                    break;

                case property_kind::string: {
                    const auto str = *reinterpret_cast<data_string*>(current.value.data);
                    writer.String(str.data, rapidjson::SizeType(str.length));
                }
                break;

                case property_kind::uuid: {
                    char buf[36];
                    static_cast<uuid*>(current.value.data)->format_to(buf);
                    writer.String(buf, 36);
                }
                break;

                default:
                    OBLO_ASSERT(false);
                    break;
                }

                break;
            default:
                OBLO_ASSERT(false);
                break;
            }

            // Let the last child close the parent
        }

        return success_tag{};
    }
}