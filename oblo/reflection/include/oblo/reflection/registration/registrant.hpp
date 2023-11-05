#pragma once

#include <oblo/core/lifetime.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/reflection/concepts/ranged_type_erasure.hpp>
#include <oblo/reflection/reflection_registry.hpp>

#include <memory>
#include <string_view>
#include <type_traits>

namespace oblo::reflection
{
    class reflection_registry::registrant
    {
    public:
        template <typename T>
        class class_builder;

    public:
        explicit registrant(reflection_registry& registry) : m_impl{*registry.m_impl} {}

        template <typename T>
        class_builder<T> add_class();

    private:
        u32 add_new_class(const type_id& type);
        void add_field(u32 classIndex, const type_id& type, std::string_view name, u32 offset);
        void add_tag(u32 classIndex, const type_id& type);
        void add_concept(u32 classIndex, const ranged_type_erasure& rte);

        template <typename T, typename U>
        static u32 get_member_offset(U(T::*m))
        {
            alignas(T) std::byte buf[sizeof(T)];
            auto& t = *start_lifetime_as<T>(buf);

            u8* const bStructPtr = reinterpret_cast<u8*>(&t);
            u8* const bMemberPtr = reinterpret_cast<u8*>(&(t.*m));
            return u32(bMemberPtr - bStructPtr);
        }

    private:
        reflection_registry_impl& m_impl;
    };

    template <typename T>
    class reflection_registry::registrant::class_builder
    {
    public:
        template <typename U>
        class_builder& add_field(U(T::*member), std::string_view name)
        {
            static_assert(std::is_standard_layout_v<T>);
            const u32 offset = get_member_offset<T>(member);
            m_registrant.add_field(m_classIndex, get_type_id<U>(), name, offset);
            return *this;
        }

        template <typename U>
        class_builder& add_tag()
        {
            m_registrant.add_tag(m_classIndex, get_type_id<T>());
            return *this;
        }

        class_builder& add_ranged_type_erasure()
        {
            ranged_type_erasure rte{
                .destroy =
                    [](void* ptr, usize count)
                {
                    T* const begin = static_cast<T*>(ptr);
                    T* const end = begin + count;

                    for (T* it = begin; it != end; ++it)
                    {
                        it->~T();
                    }
                },
            };

            if constexpr (requires { T{}; })
            {
                rte.create = [](void* dst, usize count)
                { std::uninitialized_value_construct_n(static_cast<T*>(dst), count); };
            }

            if constexpr (requires(T&& other) { T{std::move(other)}; })
            {
                rte.move = [](void* dst, void* src, usize count)
                {
                    T* outIt = static_cast<T*>(dst);
                    T* inIt = static_cast<T*>(src);
                    T* const end = outIt + count;

                    for (; outIt != end; ++outIt, ++inIt)
                    {
                        new (outIt) T(std::move(*inIt));
                    }
                };
            }

            if constexpr (requires(T& lhs, T&& rhs) { lhs = std::move(rhs); })
            {
                rte.moveAssign = [](void* dst, void* src, usize count)
                {
                    T* outIt = static_cast<T*>(dst);
                    T* inIt = static_cast<T*>(src);
                    T* const end = outIt + count;

                    for (; outIt != end; ++outIt, ++inIt)
                    {
                        *outIt = std::move(*inIt);
                    }
                };
            }

            m_registrant.add_concept(m_classIndex, rte);

            return *this;
        }

    private:
        class_builder(registrant& reg, u32 classIndex) : m_registrant{reg}, m_classIndex{classIndex} {}

    private:
        friend class registrant;

    private:
        registrant& m_registrant;
        u32 m_classIndex;
    };

    template <typename T>
    reflection_registry::registrant::class_builder<T> reflection_registry::registrant::add_class()
    {
        return {*this, add_new_class(get_type_id<T>())};
    }
}