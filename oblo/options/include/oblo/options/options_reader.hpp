#pragma once

#include <oblo/core/struct_apply.hpp>
#include <oblo/options/options_manager.hpp>

namespace oblo
{
    template <typename T>
    struct options_reader : T
    {
        static constexpr usize fields_count =
            struct_apply([]([[maybe_unused]] auto&&... m) { return sizeof...(m); }, T{});

        template <auto... values>
            requires((sizeof...(values) == fields_count))
        void init(const options_manager& opts)
        {
            const h32<option> all[fields_count] = {opts.find_option(option_traits<values>::descriptor.id)...};

            for (usize i = 0; i < fields_count; ++i)
            {
                handles[i] = all[i];
            }

            read(opts);
        }

        void read(const options_manager& opts)
        {
            struct_apply(
                [this, &opts](auto&&... m)
                {
                    usize i = 0;
                    (assign_option(m, opts.get_option_value(handles[i++])), ...);
                },
                *static_cast<T*>(this));
        }

    private:
        template <typename T>
        static void assign_option(T& val, expected<property_value_wrapper> w)
        {
            if (w)
            {
                val = w->get<T>();
            }
        }

    private:
        h32<option> handles[fields_count]{};
    };
}