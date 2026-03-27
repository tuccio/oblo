#pragma once

#include <oblo/core/data_format.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/scene/resources/animation.hpp>

namespace oblo::animation_data
{
    namespace properties
    {
        /// @brief Name for joint translation property
        constexpr string_view joint_translation = "translation";

        /// @brief Name for joint rotation property
        constexpr string_view joint_rotation = "rotation";

        /// @brief Name for joint scale property
        constexpr string_view joint_scale = "scale";
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

        inline std::span<byte> set_data_impl(
            dynamic_array<byte>& dataArray, animation_data_ref& ref, std::span<const byte> data)
        {
            const usize begin = dataArray.size();
            dataArray.append(data.data(), data.data() + data.size());
            const usize end = dataArray.size();

            ref = {.begin = begin, .end = end};

            return std::span{dataArray}.subspan(begin);
        }
    }

    inline expected<string_view> get_channel_joint_name(const animation& clip, const animation_channel& channel)
    {
        const expected data = detail::get_data_impl(clip.aligned1, channel.jointName);

        if (!data)
        {
            return data.error();
        }

        return string_view{
            reinterpret_cast<const char*>(data->data()),
            data->size(),
        };
    }

    inline void set_channel_joint_name(animation& clip, animation_channel& channel, string_view name)
    {
        detail::set_data_impl(clip.aligned1, channel.jointName, as_bytes(std::span{name}));
    }

    inline expected<string_view> get_channel_property_name(const animation& clip, const animation_channel& channel)
    {
        const expected data = detail::get_data_impl(clip.aligned1, channel.propertyName);

        if (!data)
        {
            return data.error();
        }

        return string_view{
            reinterpret_cast<const char*>(data->data()),
            data->size(),
        };
    }

    inline void set_channel_property_name(animation& clip, animation_channel& channel, string_view name)
    {
        detail::set_data_impl(clip.aligned1, channel.propertyName, as_bytes(std::span{name}));
    }

    inline expected<std::span<byte>> set_channel_data(
        animation& clip, animation_channel& channel, std::span<const byte> data, data_format format)
    {
        const auto [_, alignment] = get_size_and_alignment(format);
        std::span<byte> result{};

        switch (alignment)
        {
        case 1:
            result = detail::set_data_impl(clip.aligned1, channel.data, std::span{data});
            break;

        case 4:
            result = detail::set_data_impl(clip.aligned4, channel.data, std::span{data});
            break;

        default:
            return "Unsupported alignment value"_err;
        }

        channel.format = format;
        return result;
    }

    inline expected<std::span<const byte>> get_channel_data(const animation& clip, const animation_channel& channel)
    {
        const auto [_, alignment] = get_size_and_alignment(channel.format);

        switch (alignment)
        {
        case 1:
            return detail::get_data_impl(clip.aligned1, channel.data);

        case 4:
            return detail::get_data_impl(clip.aligned4, channel.data);

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
        const expected e = detail::get_data_impl(clip.aligned4, channel.keyframes);

        if (!e)
        {
            return e.error();
        }

        return std::span{
            reinterpret_cast<const animation_time_t*>(e->data()),
            reinterpret_cast<const animation_time_t*>(e->data() + e->size()),
        };
    }

    constexpr animation_time_t get_duration(const animation& clip)
    {
        return clip.timeEnd >= clip.timeStart ? clip.timeEnd - clip.timeStart : 0.f;
    }
}