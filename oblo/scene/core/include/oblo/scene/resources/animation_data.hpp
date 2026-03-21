#pragma once

#include <oblo/core/data_format.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/scene/resources/animation.hpp>

namespace oblo::animation_data
{
    namespace prefixes
    {
        /// @brief Prefix for animation properties that target entities
        constexpr hashed_string_view entity = "$e.";

        /// @brief Prefix for animation properties that target skeleton joints
        constexpr hashed_string_view skeletal = "$sk.";
    }

    namespace detail
    {
        inline expected<std::span<const byte>> get_data_impl(const dynamic_array<byte>& dataArray,
            const animation_data_ref& ref)
        {
            if (ref.end > dataArray.size() || ref.end < ref.begin)
            {
                return "Invalid data reference"_err;
            }

            return std::span{
                reinterpret_cast<const byte*>(dataArray.data() + ref.begin),
                ref.end - ref.begin,
            };
        }

        inline void set_data_impl(dynamic_array<byte>& dataArray, animation_data_ref& ref, std::span<const byte> data)
        {
            const usize begin = dataArray.size();
            dataArray.append(data.data(), data.data() + data.size());
            const usize end = dataArray.size();

            ref = {.begin = begin, .end = end};
        }
    }

    inline expected<hashed_string_view> get_channel_name(const animation& clip, const animation_channel& channel)
    {
        const expected data = detail::get_data_impl(clip.aligned1, channel.name);

        if (!data)
        {
            return data.error();
        }

        const string_view view{
            reinterpret_cast<const char*>(data->data()),
            data->size(),
        };

        OBLO_ASSERT(hashed_string_view{view}.hash() == channel.nameHash);

        return hashed_string_view{
            view,
            channel.nameHash,
        };
    }

    inline void set_channel_name(animation& clip, animation_channel& channel, hashed_string_view name)
    {
        detail::set_data_impl(clip.aligned1, channel.name, as_bytes(std::span{name}));
        channel.nameHash = name.hash();
    }

    inline expected<> set_channel_data(
        animation& clip, animation_channel& channel, std::span<const byte> data, data_format format)
    {
        const auto [_, alignment] = get_size_and_alignment(format);

        switch (alignment)
        {
        case 1:
            detail::set_data_impl(clip.aligned1, channel.data, std::span{data});
            break;

        case 4:
            detail::set_data_impl(clip.aligned4, channel.data, std::span{data});
            break;

        default:
            return "Unsupported alignment value"_err;
        }

        channel.format = format;
        return no_error;
    }

    inline expected<std::span<const byte>> get_channel_data(const animation& clip, const animation_channel& channel)
    {
        const auto [_, alignment] = get_size_and_alignment(channel.format);

        switch (alignment)
        {
        case 1:
            return detail::get_data_impl(clip.aligned1, channel.data);

        case 4:
            return detail::get_data_impl(clip.aligned1, channel.data);

        default:
            return "Unsupported alignment value"_err;
        }
    }

    inline void set_channel_keyframes(
        animation& clip, animation_channel& channel, std::span<const animation_time_t> data)
    {
        static_assert(alignof(animation_time_t) == 4);
        detail::set_data_impl(clip.aligned4, channel.keyframes, as_bytes(data));
    }

    inline expected<std::span<const animation_time_t>> get_channel_keyframes(const animation& clip,
        const animation_channel& channel)
    {
        static_assert(alignof(animation_time_t) == 4);
        const expected e = detail::get_data_impl(clip.aligned4, channel.data);

        if (!e)
        {
            return e.error();
        }

        return std::span{
            reinterpret_cast<const animation_time_t*>(e->data()),
            reinterpret_cast<const animation_time_t*>(e->data() + e->size()),
        };
    }
}