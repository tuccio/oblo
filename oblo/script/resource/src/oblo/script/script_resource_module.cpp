#include <oblo/core/array_size.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/reflection/codegen/registration.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/providers/resource_types_provider.hpp>
#include <oblo/script/resources/compiled_script.hpp>
#include <oblo/script/resources/traits.hpp>

#include <array>

namespace oblo
{
    namespace
    {
        class script_resource_types_provider final : public resource_types_provider
        {
        public:
            void fetch_resource_types(deque<resource_type_descriptor>& outResourceTypes) const
            {
                outResourceTypes.push_back({
                    .typeId = get_type_id<compiled_script>(),
                    .typeUuid = resource_type<compiled_script>,
                    .create = []() -> void* { return new compiled_script{}; },
                    .destroy = [](void* ptr) { delete static_cast<compiled_script*>(ptr); },
                    .load = [](void* r, cstring_view source, const any&)
                    { return load(*static_cast<compiled_script*>(r), source); },
                });
            }
        };
    }

    class script_resource_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            reflection::gen::load_module_and_register();

            initializer.services->add<script_resource_types_provider>().as<resource_types_provider>().unique();
            return true;
        }

        bool finalize() override
        {
            return true;
        }

        void shutdown() override {}
    };

    namespace
    {
        using magic_bytes_array = std::array<byte, 8>;
        using version_array = std::array<u8, 2>;

        template <usize N>
        consteval magic_bytes_array make_magic_bytes(const char (&str)[N])
        {
            std::array<std::byte, N - 1> bytes;

            for (usize i = 0; i < bytes.size(); ++i)
            {
                bytes[i] = std::byte(str[i]);
            }

            return bytes;
        }

        constexpr u16 g_ByteSwapCheck = 0x0102;
        constexpr magic_bytes_array g_MagicBytes = make_magic_bytes("oblo_bcm");

        constexpr version_array g_CurrentVersion = {0, 1};

        struct bytecode_module_header
        {
            magic_bytes_array magicBytes;
            version_array version;
            u16 byteswap;

            u32 functionsCount;
            u32 textCount;
            u32 readOnlyStringsCount;

            u32 _reserved[10];
        };

        struct bytecode_string_ref
        {
            u32 offset;
            u32 length;
        };

        struct bytecode_exported_function_data
        {
            bytecode_string_ref id;
            u32 paramsSize;
            u32 returnSize;
            u32 textOffset;
        };

        static_assert(sizeof(magic_bytes_array) == 8);
        static_assert(sizeof(bytecode_module_header) == 64);

        expected<std::span<const byte>> try_get_data(const dynamic_array<byte>& data, u32 offset, u32 size)
        {
            if (size != 0 && offset + size > data.size())
            {
                return unspecified_error;
            }

            return std::span{data.data() + offset, size};
        }

        expected<> try_read(const dynamic_array<byte>& data, u32 offset, std::span<byte> out)
        {
            if (!out.empty() && offset + out.size() >= data.size())
            {
                return unspecified_error;
            }

            std::memcpy(out.data(), data.data() + offset, out.size());
            return no_error;
        }
    }

    bool save(const compiled_script& script, cstring_view destination)
    {
        data_document doc;
        doc.init();

        doc.child_value(doc.get_root(), "bytecode"_hsv, property_value_wrapper{script.bytecode.id});
        doc.child_value(doc.get_root(), "x86_64_sse2"_hsv, property_value_wrapper{script.x86_64_sse2.id});

        return json::write(doc, destination).has_value();
    }

    bool load(compiled_script& script, cstring_view source)
    {
        data_document doc;
        doc.init();

        if (!json::read(doc, source))
        {
            return false;
        }

        const u32 bytecode = doc.find_child(doc.get_root(), "bytecode"_hsv);
        const u32 x86_64_sse2 = doc.find_child(doc.get_root(), "x86_64_sse2"_hsv);

        doc.child_value(doc.get_root(), "bytecode"_hsv, property_value_wrapper{script.bytecode.id});
        doc.child_value(doc.get_root(), "x86_64_sse2"_hsv, property_value_wrapper{script.x86_64_sse2.id});

        script = {
            .bytecode = doc.read_uuid(bytecode).value_or(uuid{}),
            .x86_64_sse2 = doc.read_uuid(x86_64_sse2).value_or(uuid{}),
        };

        return true;
    }

    bool save(const compiled_bytecode_module& script, cstring_view destination)
    {
        return save(script.module, destination);
    }

    bool save(const bytecode_module& module, cstring_view destination)
    {
        buffered_array<byte, 2048> data;
        buffered_array<char, 1024> stringsBuffer;

        {
            bytecode_module_header h{};

            h.magicBytes = g_MagicBytes;
            h.byteswap = g_ByteSwapCheck;
            h.version = g_CurrentVersion;
            h.functionsCount = module.functions.size32();
            h.textCount = module.text.size32();
            h.readOnlyStringsCount = module.readOnlyStrings.size32();

            const std::span headerData = as_bytes(std::span{&h, 1});
            data.append(headerData.begin(), headerData.end());
        }

        {
            for (const auto& f : module.functions)
            {
                const u32 idOffset = stringsBuffer.size32();
                const u32 idLength = f.id.size32();

                stringsBuffer.append(f.id.begin(), f.id.end());

                const bytecode_exported_function_data binFunction{
                    .id =
                        {
                            .offset = idOffset,
                            .length = idLength,
                        },
                    .paramsSize = f.paramsSize,
                    .returnSize = f.returnSize,
                    .textOffset = f.textOffset,
                };

                const std::span dataSpan = as_bytes(std::span{&binFunction, 1});
                data.append(dataSpan.begin(), dataSpan.end());
            }
        }

        {
            static_assert(std::is_trivially_copyable_v<decltype(module.text)::value_type>);

            const std::span dataSpan = as_bytes(std::span{module.text});
            data.append(dataSpan.begin(), dataSpan.end());
        }

        {
            for (const auto& str : module.readOnlyStrings)
            {
                const bytecode_string_ref strRef{
                    .offset = stringsBuffer.size32(),
                    .length = str.size32(),
                };

                stringsBuffer.append(str.begin(), str.end());

                const std::span dataSpan = as_bytes(std::span{&strRef, 1});
                data.append(dataSpan.begin(), dataSpan.end());
            }
        }

        return filesystem::write_file(destination, data, filesystem::write_mode::binary).has_value() &&
            filesystem::write_file(destination,
                as_bytes(std::span{stringsBuffer}),
                filesystem::write_mode::binary | filesystem::write_mode::append)
                .has_value();
    }

    bool load(compiled_bytecode_module& script, cstring_view source)
    {
        buffered_array<byte, 4096> data;

        if (!filesystem::load_binary_file_into_memory(data, source))
        {
            return false;
        }

        bytecode_module_header header{};

        if (!try_read(data, 0, as_writable_bytes(std::span{&header, 1})))
        {
            return false;
        }

        if (header.magicBytes != g_MagicBytes || header.version != g_CurrentVersion)
        {
            return false;
        }

        if (header.byteswap != g_ByteSwapCheck)
        {
            OBLO_ASSERT(false, "We could just byteswap here instead of failing");
            return false;
        }

        script.module.readOnlyStrings.clear();
        script.module.text.clear();

        constexpr u32 functionsBegin = sizeof(bytecode_module_header);
        const u32 textBegin = functionsBegin + header.functionsCount * sizeof(bytecode_exported_function_data);
        const u32 textBytesSize = header.textCount * sizeof(bytecode_instruction);
        const u32 readOnlyStringsBegin = textBegin + textBytesSize;
        const u32 stringBufferBegin = readOnlyStringsBegin + header.readOnlyStringsCount * sizeof(bytecode_string_ref);

        u32 currentOffset = functionsBegin;

        {
            script.module.functions.clear();
            script.module.functions.reserve(header.functionsCount);

            bytecode_exported_function_data functionData;
            const std::span dataSpan = as_writable_bytes(std::span{&functionData, 1});

            for (u32 i = 0; i < header.functionsCount; ++i)
            {
                if (!try_read(data, currentOffset, dataSpan))
                {
                    return false;
                }

                const expected idString =
                    try_get_data(data, stringBufferBegin + functionData.id.offset, functionData.id.length);

                if (!idString)
                {
                    return false;
                }

                auto& f = script.module.functions.push_back_default();
                f.id = string{reinterpret_cast<const char*>(idString->data()), idString->size()};
                f.paramsSize = functionData.paramsSize;
                f.returnSize = functionData.returnSize;
                f.textOffset = functionData.textOffset;

                currentOffset += sizeof(bytecode_exported_function_data);
            }
        }

        if (currentOffset != textBegin)
        {
            return false;
        }

        {
            script.module.text.clear();
            script.module.text.reserve(header.textCount);

            const expected textBytes = try_get_data(data, currentOffset, textBytesSize);

            if (!textBytes)
            {
                return false;
            }

            script.module.text.resize_default(header.textCount);
            OBLO_ASSERT(script.module.text.size_bytes() == textBytesSize);
            std::memcpy(script.module.text.data(), textBytes->data(), textBytesSize);

            currentOffset += textBytesSize;
        }

        if (currentOffset != readOnlyStringsBegin)
        {
            return false;
        }

        {
            script.module.readOnlyStrings.clear();
            script.module.readOnlyStrings.reserve(header.readOnlyStringsCount);

            bytecode_string_ref stringRef;
            const std::span dataSpan = as_writable_bytes(std::span{&stringRef, 1});

            for (u32 i = 0; i < header.readOnlyStringsCount; ++i)
            {
                if (!try_read(data, currentOffset, dataSpan))
                {
                    return false;
                }

                const expected idString = try_get_data(data, stringBufferBegin + stringRef.offset, stringRef.length);

                if (!idString)
                {
                    return false;
                }

                script.module.readOnlyStrings.emplace_back(reinterpret_cast<const char*>(idString->data()),
                    idString->size());

                currentOffset += sizeof(bytecode_string_ref);
            }
        }

        return true;
    }

    bool load(compiled_native_module& script, cstring_view source)
    {
        return script.module.open(source);
    }
}

OBLO_MODULE_REGISTER(oblo::script_resource_module)