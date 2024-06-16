#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/color.hpp>
#include <oblo/thread/parallel_for.hpp>

#include <span>
#include <type_traits>

namespace oblo::importers::image_processing
{
    enum class component
    {
        red,
        green,
        blue,
        alpha
    };

    template <component... Components>
    struct component_swizzle
    {
        static constexpr u32 count()
        {
            return sizeof...(Components);
        }

        template <component... OtherComponents, typename F, typename P>
        static constexpr void for_each(F&& f, const P& pixel)
        {
            (apply<find<OtherComponents>()>(f, pixel), ...);
        }

        template <u32 Index, typename F, typename P>
        static constexpr void apply([[maybe_unused]] F&& f, [[maybe_unused]] const P& pixel)
        {
            if constexpr (Index != ~u32{})
            {
                f(pixel[Index], Index);
            }
        }

        template <component Target>
        static constexpr u32 find()
        {
            constexpr component components[] = {Components...};

            for (u32 i = 0; i < count(); ++i)
            {
                if (components[i] == Target)
                {
                    return i;
                }
            }

            return ~u32{};
        }
    };

    namespace swizzle
    {
        using r = component_swizzle<component::red>;
        using rg = component_swizzle<component::red, component::green>;
        using rgb = component_swizzle<component::red, component::green, component::blue>;
        using rgba = component_swizzle<component::red, component::green, component::blue, component::alpha>;
    }

    template <typename T, typename Swizzle>
    class image_view
    {
    public:
        static constexpr u32 Components = Swizzle::count();
        using byte_type = std::conditional_t<std::is_const_v<T>, const byte, byte>;
        using pixel_view = std::span<T, Components>;
        using swizzle = Swizzle;

        image_view() = default;
        image_view(const image_view&) = default;

        explicit image_view(std::span<byte_type> data, u32 width, u32 height) : m_width{width}, m_height{height}
        {
            OBLO_ASSERT(data.size() % (sizeof(T) * Components) == 0);
            OBLO_ASSERT(uintptr(data.data()) % alignof(T) == 0);

            m_view = std::span{reinterpret_cast<T*>(data.data()), data.size() / sizeof(T)};

            m_rowPitch = round_up_multiple(width * Components, 4u);
        }

        image_view& operator=(const image_view&) = default;

        pixel_view at(u32 row, u32 column) const
        {
            OBLO_ASSERT(row < m_height && column < m_width);

            const u32 offset = m_rowPitch * row + column * Components;
            return m_view.subspan(offset).template subspan<0, Components>();
        }

        u32 get_width() const
        {
            return m_width;
        }

        u32 get_height() const
        {
            return m_height;
        }

    private:
        std::span<T> m_view;
        u32 m_width{};
        u32 m_height{};
        u32 m_rowPitch{};
    };

    template <typename T>
    using image_view_r = image_view<T, swizzle::r>;

    template <typename T>
    using image_view_rg = image_view<T, swizzle::rg>;

    template <typename T>
    using image_view_rgb = image_view<T, swizzle::rgb>;

    template <typename T>
    using image_view_rgba = image_view<T, swizzle::rgba>;

    template <typename T, typename Swizzle, typename F>
    void for_each_pixel(image_view<T, Swizzle> image, F&& f)
    {
        const u32 w = image.get_width();
        const u32 h = image.get_height();

        for (u32 i = 0; i < w; ++i)
        {
            for (u32 j = 0; j < h; ++j)
            {
                f(i, j, image.at(i, j));
            }
        }
    }

    template <typename T, typename Swizzle, typename F>
    void parallel_for_each_pixel(image_view<T, Swizzle> image, F&& f)
    {
        const u32 h = image.get_height();
        const u32 w = image.get_width();

        parallel_for_2d(
            [&image, &f](job_range rows, job_range cols)
            {
                for (u32 i = rows.begin; i < rows.end; ++i)
                {
                    for (u32 j = cols.begin; j < cols.end; ++j)
                    {
                        f(i, j, image.at(i, j));
                    }
                }
            },
            job_range{0, h},
            job_range{0, w},
            64,
            64);
    }

    template <typename ColorSpace, typename T, typename Swizzle>
    struct box_filter_2x2
    {
        box_filter_2x2(ColorSpace, image_view<const T, Swizzle> previous) : previousLevel{previous} {}

        image_view<const T, Swizzle> previousLevel;

        void operator()(u32 i, u32 j, image_view<T, Swizzle>::pixel_view out) const
        {
            const u32 mipRow = 2 * i;
            const u32 mipColumn = 2 * j;

            u32 samples{};

            f32 sum[Swizzle::count()]{};

            for (u32 ip = mipRow; ip < mipRow + 2; ++ip)
            {
                if (ip >= previousLevel.get_height())
                {
                    continue;
                }

                for (u32 jp = mipColumn; jp < mipColumn + 2; ++jp)
                {
                    if (jp >= previousLevel.get_width())
                    {
                        continue;
                    }

                    ++samples;

                    const auto source = previousLevel.at(ip, jp);

                    Swizzle::template for_each<component::red, component::green, component::blue>(
                        [&sum](const T& c, u32 index)
                        {
                            const f32 f = color_convert_linear_f32(ColorSpace{}, c);
                            sum[index] += f;
                        },
                        source);

                    Swizzle::template for_each<component::alpha>(
                        [&sum](const T& c, u32 index)
                        {
                            // We assume alpha is always linear
                            const f32 f = color_convert_linear_f32(linear_color_tag{}, c);
                            sum[index] += f;
                        },
                        source);
                }
            }

            Swizzle::template for_each<component::red, component::green, component::blue>(
                [&sum, samples](T& c, u32 index)
                {
                    const f32 avg = sum[index] / f32(samples);
                    c = color_convert<T>(linear_color_tag{}, ColorSpace{}, avg);
                },
                out);

            Swizzle::template for_each<component::alpha>(
                [&sum, samples](T& c, u32 index)
                {
                    const f32 avg = sum[index] / f32(samples);
                    c = color_convert<T>(linear_color_tag{}, linear_color_tag{}, avg);
                },
                out);
        }
    };

    template <typename ColorSpace, typename T, typename Swizzle>
    box_filter_2x2(ColorSpace, image_view<const T, Swizzle>) -> box_filter_2x2<ColorSpace, T, Swizzle>;
}