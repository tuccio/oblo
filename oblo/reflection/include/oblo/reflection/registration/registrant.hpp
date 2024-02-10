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
    template <typename T>
    struct concept_type;

    template <typename T>
    struct tag_type;

    class reflection_registry::registrant
    {
    public:
        template <typename T>
        class class_builder;

    public:
        explicit registrant(reflection_registry& registry) : m_impl{*registry.m_impl} {}

        template <typename T>
        class_builder<T> add_class();

        template <typename T>
            requires std::is_fundamental_v<T>
        void add_fundamental();

    private:
        u32 add_new_type(const type_id& type, u32 size, u32 alignment, type_kind kind);

        template <typename T>
        u32 add_new_type(type_kind kind)
        {
            return add_new_type(get_type_id<T>(), sizeof(T), alignof(T), kind);
        }

        void add_field(u32 entityIndex, const type_id& type, std::string_view name, u32 offset);
        void add_tag(u32 entityIndex, const type_id& type);
        void add_concept(
            u32 entityIndex, const type_id& type, u32 size, u32 alignment, const ranged_type_erasure& rte, void* src);

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
            const u32 offset = get_member_offset<T>(member);
            m_registrant.add_field(m_entityIndex, get_type_id<U>(), name, offset);
            return *this;
        }

        template <typename U>
        class_builder& add_tag()
        {
            m_registrant.add_tag(m_entityIndex, get_type_id<tag_type<U>>());
            return *this;
        }

        template <typename C>
        class_builder& add_concept(C value)
        {
            m_registrant.add_concept(m_entityIndex,
                get_type_id<concept_type<C>>(),
                sizeof(C),
                alignof(C),
                make_ranged_type_erasure<C>(),
                &value);

            return *this;
        }

        class_builder& add_ranged_type_erasure()
        {
            return add_concept(make_ranged_type_erasure<T>());
        }

    private:
        class_builder(registrant& reg, u32 entityIndex) : m_registrant{reg}, m_entityIndex{entityIndex} {}

    private:
        friend class registrant;

    private:
        registrant& m_registrant;
        u32 m_entityIndex;
    };

    template <typename T>
    reflection_registry::registrant::class_builder<T> reflection_registry::registrant::add_class()
    {
        return {*this, add_new_type<T>(type_kind::class_kind)};
    }

    template <typename T>
        requires std::is_fundamental_v<T>
    void reflection_registry::registrant::add_fundamental()
    {
        add_new_type<T>(type_kind::fundamental_kind);
    }
}