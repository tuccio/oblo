#pragma once

#include <oblo/core/struct_apply.hpp>
#include <oblo/options/options_manager.hpp>

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

    private:
        h32<option> m_handle{};
    };

    template <typename T>
    struct option_proxy_struct : T
    {
        static void register_options(options_manager& manager)
        {
            struct_apply([&manager]<fixed_string... Names>(const option_proxy<Names>&...)
                { (manager.register_option(option_proxy<Names>::traits::descriptor), ...); },
                T{});
        }

        void init(const options_manager& manager)
        {
            struct_apply([&manager]<fixed_string... Names>(auto&... proxy) { (proxy.init(manager), ...); },
                *static_cast<T*>(this));
        }
    };
}