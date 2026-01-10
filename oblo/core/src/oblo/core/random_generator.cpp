#include <oblo/core/random_generator.hpp>

#include <oblo/core/reflection/fields.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>

#include <random>

namespace oblo
{
    using underlying_random_generator = std::mt19937;

    struct random_generator::impl : underlying_random_generator
    {
    };

    static_assert(random_generator::min() == underlying_random_generator::min());
    static_assert(random_generator::max() == underlying_random_generator::max());

    random_generator::random_generator() = default;

    random_generator::random_generator(random_generator&&) noexcept = default;

    random_generator& random_generator::operator=(random_generator&&) noexcept = default;

    random_generator::~random_generator() = default;

    u32 random_generator::seed()
    {
        std::random_device dev;
        const u32 s = dev();
        seed(s);
        return s;
    }

    u32 random_generator::seed(u32 seed)
    {
        if (!m_impl)
        {
            m_impl = allocate_unique<impl>();
        }

        m_impl->seed(seed);
        return seed;
    }

    u32 random_generator::generate()
    {
        return m_impl->operator()();
    }

    u32 random_generator::operator()()
    {
        return m_impl->operator()();
    }

    template <typename T>
    T uniform_distribution<T>::generate(random_generator& gen) const
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            std::uniform_real_distribution<T> dist{min, max};
            return dist(gen);
        }
        else if constexpr (std::is_integral_v<T>)
        {
            std::uniform_int_distribution<T> dist{min, max};
            return dist(gen);
        }
        else if constexpr (constexpr auto n = count_fields<T>(); n == 2)
        {
            using U = decltype(min.x);

            return {
                uniform_distribution<U>{min.x, max.x}.generate(gen),
                uniform_distribution<U>{min.y, max.y}.generate(gen),
            };
        }
        else if constexpr (n == 3)
        {
            using U = decltype(min.x);

            return {
                uniform_distribution<U>{min.x, max.x}.generate(gen),
                uniform_distribution<U>{min.y, max.y}.generate(gen),
                uniform_distribution<U>{min.z, max.z}.generate(gen),
            };
        }
        else if constexpr (n == 4)
        {
            using U = decltype(min.x);

            return {
                uniform_distribution<U>{min.x, max.x}.generate(gen),
                uniform_distribution<U>{min.y, max.y}.generate(gen),
                uniform_distribution<U>{min.z, max.z}.generate(gen),
                uniform_distribution<U>{min.w, max.w}.generate(gen),
            };
        }
    }

    template struct uniform_distribution<f32>;
    template struct uniform_distribution<u32>;
    template struct uniform_distribution<u64>;

    template struct uniform_distribution<vec2>;
    template struct uniform_distribution<vec2u>;
    template struct uniform_distribution<vec3>;
    template struct uniform_distribution<vec4>;
}