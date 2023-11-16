#include <oblo/properties/serialization/json.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/file_utility.hpp>
#include <oblo/core/small_vector.hpp>
#include <oblo/core/types.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/data_node.hpp>

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
                expect_object_start,
                expect_name_or_object_end,
                expect_value,
            };

            data_document& m_doc;
            state m_state{state::expect_object_start};
            std::string m_lastString;

            std::vector<u32> m_stack;

            explicit Handler(data_document& doc) : m_doc{doc}
            {
                m_stack.reserve(32);
                m_stack.assign(1, m_doc.get_root());
            }

            bool StartObject()
            {
                switch (m_state)
                {
                case state::expect_object_start:
                    m_state = state::expect_name_or_object_end;

                    if (!m_lastString.empty())
                    {
                        // TODO: Add object
                        OBLO_ASSERT(false);
                        m_lastString.clear();
                    }
                    return true;
                default:
                    return false;
                }
            }

            bool String(const char* str, rapidjson::SizeType length, bool)
            {
                switch (m_state)
                {
                case state::expect_name_or_object_end:
                    m_lastString.assign(str, length);
                    m_state = state::expect_value;
                    return true;
                case state::expect_value:
                    // TODO: String value
                    OBLO_ASSERT(false);
                    m_state = state::expect_name_or_object_end;
                    return true;
                default:
                    return false;
                }
            }

            bool Bool(bool value)
            {
                switch (m_state)
                {
                case state::expect_value:
                    m_doc.child_value(m_stack.back(),
                        m_lastString,
                        property_kind::boolean,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = state::expect_name_or_object_end;
                    return true;
                default:
                    return false;
                }
            }

            bool Int(i32 value)
            {
                switch (m_state)
                {
                case state::expect_value:
                    m_doc.child_value(m_stack.back(),
                        m_lastString,
                        property_kind::i32,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = state::expect_name_or_object_end;
                    return true;
                default:
                    return false;
                }
            }

            bool Uint(u32 value)
            {
                switch (m_state)
                {
                case state::expect_value:
                    m_doc.child_value(m_stack.back(),
                        m_lastString,
                        property_kind::u32,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = state::expect_name_or_object_end;
                    return true;
                default:
                    return false;
                }
            }

            bool Int64(i64 value)
            {
                switch (m_state)
                {
                case state::expect_value:
                    m_doc.child_value(m_stack.back(),
                        m_lastString,
                        property_kind::i64,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = state::expect_name_or_object_end;
                    return true;
                default:
                    return false;
                }
            }

            bool UiInt64(u64 value)
            {
                switch (m_state)
                {
                case state::expect_value:
                    m_doc.child_value(m_stack.back(),
                        m_lastString,
                        property_kind::u64,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = state::expect_name_or_object_end;
                    return true;
                default:
                    return false;
                }
            }

            bool Double(f64 value)
            {
                switch (m_state)
                {
                case state::expect_value:
                    m_doc.child_value(m_stack.back(),
                        m_lastString,
                        property_kind::f64,
                        std::as_bytes(std::span{&value, 1}));

                    m_lastString.clear();
                    m_state = state::expect_name_or_object_end;
                    return true;
                default:
                    return false;
                }
            }

            bool EndObject(rapidjson::SizeType)
            {
                return m_state == state::expect_name_or_object_end;
            }

            bool Default()
            {
                OBLO_ASSERT(false);
                return false;
            }
        };
    }

    bool read(data_document& doc, const std::filesystem::path& source)
    {
        const auto file = file_ptr{open_file(source, "r")};

        constexpr auto bufferSize{1024};
        char buffer[bufferSize];

        rapidjson::FileReadStream rs{file.get(), buffer, bufferSize};

        rapidjson::Reader reader{};

        doc.init();

        reader.IterativeParseInit();

        Handler handler{doc};

        while (!reader.IterativeParseComplete())
        {
            reader.IterativeParseNext<rapidjson::kParseDefaultFlags>(rs, handler);
        }

        return !reader.HasParseError();
    }

    bool write(const data_document& doc, const std::filesystem::path& destination)
    {
        const u32 root = doc.get_root();

        if (root == data_node::Invalid)
        {
            return false;
        }

        const auto file = file_ptr{open_file(destination, "w")};

        constexpr auto bufferSize{1024};
        char buffer[bufferSize];

        rapidjson::FileWriteStream ws{file.get(), buffer, bufferSize};
        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer{ws};

        const std::span nodes = doc.get_nodes();

        const auto visit = [&](auto&& recurse, u32 node) -> void
        {
            if (node == data_node::Invalid)
            {
                return;
            }

            const auto& current = nodes[node];
            switch (current.kind)
            {
            case data_node_kind::object:
                if (current.keyLen != 0)
                {
                    writer.String(current.key, current.keyLen);
                }

                if (writer.StartObject())
                {
                    recurse(recurse, current.object.firstChild);
                    writer.EndObject();
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
                    writer.Int(*static_cast<i64*>(current.value.data));
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

                default:
                    OBLO_ASSERT(false);
                    break;
                }

                break;
            default:
                OBLO_ASSERT(false);
                break;
            }

            recurse(recurse, current.nextSibling);
        };

        visit(visit, root);

        return true;
    }
}