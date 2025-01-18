#pragma once

#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/fixed_string.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/options/option_traits.hpp>
#include <oblo/options/options_manager.hpp>
#include <oblo/options/options_provider.hpp>

namespace oblo
{
    template <fixed_string name>
    class option_proxy
    {
    public:
        using traits = option_traits<name>;
        using type = typename traits::type;

        void init(const options_manager& opts)
        {
            m_handle = opts.find_option(traits::descriptor.id);
        }

        type read(const options_manager& opts) const
        {
            return opts.get_option_value(m_handle)->get<type>();
        }

        constexpr const option_descriptor& descriptor() const
        {
            return traits::descriptor;
        }

    private:
        h32<option> m_handle{};
    };

    template <typename... T>
    struct option_proxy_struct : T...
    {
        static void register_options(service_registry& services)
        {
            using lambda_provider = lambda_options_provider<decltype([](deque<option_descriptor>& out) {
        (struct_apply([&out]<fixed_string... Names>(const option_proxy<Names>&...)
                { (out.push_back(option_proxy<Names>::traits::descriptor), ...); },
                T{}), ...);
            })>;

            services.add<lambda_provider>().template as<options_provider>().unique();
        }

        void init(const options_manager& manager)
        {
            (struct_apply([&manager](auto&... proxy) { (proxy.init(manager), ...); }, *static_cast<T*>(this)), ...);
        }

        template <typename U>
        void read(const options_manager& manager, U& out)
        {
            (struct_apply([&manager, &out](auto&... proxy)
                 { struct_apply([&proxy..., &manager](auto&... o) { ((o = proxy.read(manager)), ...); }, out); },
                 *static_cast<T*>(this)),
                ...);
        }
    };
}