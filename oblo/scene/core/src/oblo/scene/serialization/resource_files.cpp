#include <oblo/scene/serialization/animation_file.hpp>
#include <oblo/scene/serialization/model_file.hpp>
#include <oblo/scene/serialization/skeleton_file.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/helpers.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/scene/resources/animation.hpp>
#include <oblo/scene/resources/model.hpp>
#include <oblo/scene/resources/skeleton.hpp>

#include <array>

namespace oblo
{
    namespace
    {
        template <typename T>
        void write_ref_array(
            data_document& doc, u32 parent, hashed_string_view name, const dynamic_array<resource_ref<T>>& array)
        {
            const auto node = doc.child_array(parent, name);

            for (const auto& ref : array)
            {
                const auto v = doc.array_push_back(node);
                doc.make_value(v, property_kind::uuid, as_bytes(ref.id));
            }
        }

        template <typename T>
        bool read_ref_array(
            data_document& doc, u32 parent, hashed_string_view name, dynamic_array<resource_ref<T>>& array)
        {
            const auto node = doc.find_child(parent, name);

            if (node == data_node::Invalid || !doc.is_array(node))
            {
                return false;
            }

            for (u32 child = doc.child_next(node, data_node::Invalid); child != data_node::Invalid;
                child = doc.child_next(node, child))
            {
                const uuid id = doc.read_uuid(child).value_or({});
                array.emplace_back(id);
            }

            return true;
        }
    }

    expected<> save_model_json(const model& model, cstring_view destination)
    {
        data_document doc;
        doc.init();

        write_ref_array(doc, doc.get_root(), "meshes"_hsv, model.meshes);
        write_ref_array(doc, doc.get_root(), "materials"_hsv, model.materials);

        return json::write(doc, destination);
    }

    expected<> load_model(model& model, cstring_view source)
    {
        data_document doc;

        if (auto e = json::read(doc, source); !e)
        {
            return e;
        }

        read_ref_array(doc, doc.get_root(), "meshes"_hsv, model.meshes);
        read_ref_array(doc, doc.get_root(), "materials"_hsv, model.materials);

        return no_error;
    }

    expected<> save_skeleton_json(const skeleton& sk, cstring_view destination)
    {
        data_document doc;
        doc.init();

        const u32 joints = doc.child_array(doc.get_root(), "joints"_hsv, sk.jointsHierarchy.size32());

        u32 currentJointNode = data_node::Invalid;

        for (u32 i = 0; i < sk.jointsHierarchy.size32(); ++i)
        {
            const auto& joint = sk.jointsHierarchy[i];
            currentJointNode = doc.child_next(joints, currentJointNode);

            doc.make_object(currentJointNode);
            doc.child_value(currentJointNode, "name"_hsv, property_value_wrapper{string_view{joint.name}});
            doc.child_value(currentJointNode, "parentIndex"_hsv, property_value_wrapper{joint.parentIndex});

            write_child_array(doc, currentJointNode, "translation"_hsv, std::span{&joint.translation[0], 3});
            write_child_array(doc, currentJointNode, "rotation"_hsv, std::span{&joint.rotation[0], 4});
            write_child_array(doc, currentJointNode, "scale"_hsv, std::span{&joint.scale[0], 3});
        }

        return json::write(doc, destination);
    }

    expected<> load_skeleton(skeleton& sk, cstring_view source)
    {
        data_document doc;

        if (auto e = json::read(doc, source); !e)
        {
            return e;
        }

        const u32 joints = doc.find_child(doc.get_root(), "joints"_hsv);

        if (joints == data_node::Invalid || !doc.is_array(joints))
        {
            return "Invalid skeleton file"_err;
        }

        sk.jointsHierarchy.reserve(doc.children_count(joints));

        for (u32 child : doc.children(joints))
        {
            auto& joint = sk.jointsHierarchy.emplace_back();

            const u32 nameNode = doc.find_child(child, "name"_hsv);
            const u32 parentIndexNode = doc.find_child(child, "parentIndex"_hsv);

            const expected name = doc.read_string(nameNode);
            const expected parentIndex = doc.read_u32(parentIndexNode);

            const expected position =
                read_child_array(doc, child, "translation"_hsv, std::span{&joint.translation[0], 3});

            const expected rotation = read_child_array(doc, child, "rotation"_hsv, std::span{&joint.rotation[0], 4});

            const expected scale = read_child_array(doc, child, "scale"_hsv, std::span{&joint.scale[0], 3});

            if (!name || !parentIndex || !position || !rotation || !scale ||
                *parentIndex != skeleton::joint::no_parent && *parentIndex >= sk.jointsHierarchy.size())
            {
                return "Invalid data in skeleton"_err;
            }

            joint.name = string{name->str()};
            joint.parentIndex = parentIndex.value();
        }

        return no_error;
    }

    expected<> save_skin_json(const skin& sk, cstring_view destination)
    {
        data_document doc;
        doc.init();

        const u32 numJoints = sk.jointNames.size32();

        if (numJoints != sk.invBindPoses.size32())
        {
            return "Invalid skin data"_err;
        }

        doc.child_value(doc.get_root(), "skeleton"_hsv, property_value_wrapper{sk.skeleton.id});

        write_child_array(doc, doc.get_root(), "joints"_hsv, std::span{sk.jointNames});
        const u32 invBindPoses = doc.child_array(doc.get_root(), "invBindPoses"_hsv);

        for (u32 i = 0; i < numJoints; ++i)
        {
            write_child_array(doc, invBindPoses, {}, std::span{&sk.invBindPoses[i].columns[0][0], 16});
        }

        return json::write(doc, destination);
    }

    expected<> load_skin(skin& sk, cstring_view source)
    {
        data_document doc;

        if (auto e = json::read(doc, source); !e)
        {
            return e;
        }

        const u32 skeleton = doc.find_child(doc.get_root(), "skeleton"_hsv);
        const u32 joints = doc.find_child(doc.get_root(), "joints"_hsv);
        const u32 invBindPoses = doc.find_child(doc.get_root(), "invBindPoses"_hsv);

        if (joints == data_node::Invalid || !doc.is_array(joints) || invBindPoses == data_node::Invalid ||
            !doc.is_array(invBindPoses) || skeleton == data_node::Invalid)
        {
            return "Invalid skin file"_err;
        }

        const uuid skeletonId = doc.read_uuid(skeleton).value_or({});
        sk.skeleton = resource_ref<oblo::skeleton>{skeletonId};

        const u32 numJoints = doc.children_count(joints);

        if (numJoints != doc.children_count(invBindPoses))
        {
            return "Invalid skin data"_err;
        }

        sk.jointNames.resize(numJoints);
        if (!read_array(doc, joints, std::span{sk.jointNames}))
        {
            return "Invalid skin data"_err;
        }

        sk.invBindPoses.resize_default(numJoints);

        u32 currentBindPoseIdx = 0;

        for (const u32 child : doc.children(invBindPoses))
        {
            if (!read_array(doc, child, std::span{&sk.invBindPoses[currentBindPoseIdx].columns[0][0], 16}))
            {
                return "Invalid skin data"_err;
            }

            ++currentBindPoseIdx;
        }

        return no_error;
    }

    namespace
    {
        enum class animation_file_flag : u8
        {
            little_endian,
            enum_max = 8u,
        };

        using animation_file_flags_t = flags<animation_file_flag>;
        static_assert(sizeof(animation_file_flags_t) == 1u);

        struct animation_file_ref
        {
            u64 begin;
            u64 end;

            static animation_data_ref deserialize(const animation_file_ref& r)
            {
                return {narrow_cast<usize>(r.begin), narrow_cast<usize>(r.end)};
            }

            static animation_file_ref serialize(const animation_data_ref& r)
            {
                return {u64{r.begin}, u64{r.end}};
            }
        };

        enum class animation_file_array : u8
        {
            aligned1,
            align2,
            aligned4,
            align8,
            aligned16,
            align32,
        };

        struct animation_file_header
        {
            static constexpr u32 max_arrays = 16;
            static constexpr std::array<char, 4> magic_fourtet{'o', 'a', 'n', 'i'};

            using ref = animation_file_ref;

            struct version
            {
                u8 major;
                u8 minor;

                constexpr bool operator==(const version&) const = default;
            };

            std::array<char, 4> fourtet;
            version fileVersion;
            animation_file_flags_t flags;
            u8 _padding0[1];

            u32 numChannels;
            animation_time_t timeStart;
            animation_time_t timeEnd;

            ref keyframes;
            ref arrays[max_arrays];
        };

        struct animation_file_channel
        {
            using ref = animation_file_ref;

            ref propertyName;
            ref propertyArrayIndices;
            ref data;
            ref keyframes;
            data_format format;
            animation_data_kind dataKind;
            animation_target target;
            animation_interpolation interpolation;
            union {
                uuid componentUuid;
                ref jointName;
            };
        };

        constexpr animation_file_header::version current_animation_version{0, 1};

        template <animation_file_array Kind>
        constexpr auto get_animation_member_array() -> dynamic_array<byte>(animation::*)
        {
            if constexpr (Kind == animation_file_array::aligned1)
            {
                return &animation::aligned1;
            }

            else if constexpr (Kind == animation_file_array::aligned4)
            {
                return &animation::aligned4;
            }
        }

        template <animation_file_array Kind>
        void add_animation_file_header_reference(
            animation_file_header& header, const animation& anim, u64& currentOffset)
        {
            constexpr auto array = get_animation_member_array<Kind>();

            const animation_file_ref dataRef = {
                currentOffset,
                currentOffset + (anim.*array).size(),
            };

            header.arrays[u32(Kind)] = dataRef;
            currentOffset = dataRef.end;
        };
    }

    expected<> save_animation(const animation& anim, cstring_view destination)
    {
        animation_file_header header{
            .fourtet = animation_file_header::magic_fourtet,
            .fileVersion = current_animation_version,
            .numChannels = anim.channels.size32(),
            .timeStart = anim.timeStart,
            .timeEnd = anim.timeEnd,
        };

        if (anim.endianness == platform::endian::little)
        {
            header.flags |= animation_file_flag::little_endian;
        }

        u64 currentOffset = 0u;
        add_animation_file_header_reference<animation_file_array::aligned1>(header, anim, currentOffset);
        add_animation_file_header_reference<animation_file_array::aligned4>(header, anim, currentOffset);

        const filesystem::file_ptr out{filesystem::open_file(destination, "wb")};

        if (!out)
        {
            return "Failed to open file for writing"_err;
        }

        if (fwrite(&header, 1u, sizeof(animation_file_header), out.get()) != sizeof(animation_file_header))
        {
            return "Failed to write animation header"_err;
        }

        // Write the data, we need to respect the order we used for add_animation_file_header_reference
        if (fwrite(anim.aligned1.data(), 1u, anim.aligned1.size_bytes(), out.get()) != anim.aligned1.size_bytes() ||
            fwrite(anim.aligned4.data(), 1u, anim.aligned4.size_bytes(), out.get()) != anim.aligned4.size_bytes())
        {
            return "Failed to write animation data"_err;
        }

        constexpr u32 channelsBufferCount = 32;
        animation_file_channel fileChannels[channelsBufferCount];

        for (u32 srcChannelIdx = 0; srcChannelIdx < header.numChannels;)
        {
            u32 writtenChannels = 0;

            for (u32 dstChannelIdx = 0; dstChannelIdx < channelsBufferCount && srcChannelIdx < header.numChannels;
                ++dstChannelIdx, ++srcChannelIdx)
            {
                const animation_channel& srcChannel = anim.channels[srcChannelIdx];

                fileChannels[dstChannelIdx] = {
                    .propertyName = animation_file_ref::serialize(srcChannel.propertyName),
                    .propertyArrayIndices = animation_file_ref::serialize(srcChannel.propertyArrayIndices),
                    .data = animation_file_ref::serialize(srcChannel.data),
                    .keyframes = animation_file_ref::serialize(srcChannel.keyframes),
                    .format = srcChannel.format,
                    .dataKind = srcChannel.dataKind,
                    .target = srcChannel.target,
                    .interpolation = srcChannel.interpolation,
                };

                switch (srcChannel.target)
                {
                case animation_target::component:
                    fileChannels[dstChannelIdx].componentUuid = srcChannel.componentUuid;
                    break;

                case animation_target::joint:
                    fileChannels[dstChannelIdx].jointName = animation_file_ref::serialize(srcChannel.jointName);
                    break;

                default:
                    return "Invalid target type"_err;
                }

                ++writtenChannels;
            }

            if (writtenChannels > 0 &&
                fwrite(fileChannels, sizeof(animation_file_channel), writtenChannels, out.get()) != writtenChannels)
            {
                return "Failed to write animation channel data"_err;
            }
        }

        return no_error;
    }

    expected<> load_animation(animation& anim, cstring_view source)
    {
        const filesystem::file_ptr in{filesystem::open_file(source, "rb")};

        if (!in)
        {
            return "Failed to open file for reading"_err;
        }

        // Read and validate the header
        animation_file_header header;
        if (fread(&header, sizeof(animation_file_header), 1, in.get()) != 1)
        {
            return "Failed to read animation header"_err;
        }

        if (header.fourtet != animation_file_header::magic_fourtet)
        {
            return "Invalid animation file format (Magic mismatch)"_err;
        }

        if (header.fileVersion != current_animation_version)
        {
            return "Incompatible animation file version"_err;
        }

        anim.timeStart = header.timeStart;
        anim.timeEnd = header.timeEnd;

        anim.endianness = header.flags.contains(animation_file_flag::little_endian) ? platform::endian::little
                                                                                    : platform::endian::big;

        // Allocate the data arrays
        const animation_file_ref aligned1ArrayRef = header.arrays[u32(animation_file_array::aligned1)];
        const animation_file_ref aligned4ArrayRef = header.arrays[u32(animation_file_array::aligned4)];

        const usize sizeAligned1 = aligned1ArrayRef.end - aligned1ArrayRef.begin;
        const usize sizeAligned4 = aligned4ArrayRef.end - aligned4ArrayRef.begin;

        anim.aligned1.resize_default(sizeAligned1);
        anim.aligned4.resize_default(sizeAligned4);

        if (fread(anim.aligned1.data(), 1, sizeAligned1, in.get()) != sizeAligned1 ||
            fread(anim.aligned4.data(), 1, sizeAligned4, in.get()) != sizeAligned4)
        {
            return "Failed to read animation data blocks"_err;
        }

        // Read the channels
        anim.channels.resize(header.numChannels);

        constexpr u32 channelsBufferCount = 32;
        animation_file_channel fileChannels[channelsBufferCount];

        for (u32 channelIdx = 0; channelIdx < header.numChannels;)
        {
            const u32 toRead = min(channelsBufferCount, header.numChannels - channelIdx);

            if (fread(fileChannels, sizeof(animation_file_channel), toRead, in.get()) != toRead)
            {
                return "Failed to read animation channel metadata"_err;
            }

            for (u32 i = 0; i < toRead; ++i, ++channelIdx)
            {
                const animation_file_channel& src = fileChannels[i];
                animation_channel& dst = anim.channels[channelIdx];

                // Reconstruct the views/references
                dst.propertyName = animation_file_ref::deserialize(src.propertyName);
                dst.propertyArrayIndices = animation_file_ref::deserialize(src.propertyArrayIndices);
                dst.data = animation_file_ref::deserialize(src.data);
                dst.keyframes = animation_file_ref::deserialize(src.keyframes);
                dst.format = src.format;
                dst.dataKind = src.dataKind;
                dst.target = src.target;
                dst.interpolation = src.interpolation;

                switch (src.target)
                {
                case animation_target::joint:
                    dst.jointName = animation_file_ref::deserialize(src.jointName);
                    break;

                case animation_target::component:
                    dst.componentUuid = src.componentUuid;
                    break;

                default:
                    return "Invalid animation target"_err;
                }
            }
        }

        return no_error;
    }
}