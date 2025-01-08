#pragma once

#include <oblo/core/deque.hpp>

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
    class lambda_provider_service : public provider_service<T>, F
    {
    public:
        void fetch(deque<T>& out) const override
        {
            F::operator()(out);
        }
    };
}