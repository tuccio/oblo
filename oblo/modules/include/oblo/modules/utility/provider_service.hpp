#pragma once

#include <oblo/core/deque.hpp>

#include <type_traits>

namespace oblo
{
    template <typename T>
    class provider_service
    {
    public:
        virtual ~provider_service() = default;

        virtual void fetch(deque<T>& out) const = 0;
    };

    template <typename T, typename F>
    class lambda_provider_service : public provider_service<T>, std::remove_const_t<F>
    {
    public:
        void fetch(deque<T>& out) const override
        {
            F::operator()(out);
        }
    };
}