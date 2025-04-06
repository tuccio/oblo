#pragma once

#include <oblo/core/lifetime.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/reflection/concepts/handle_concept.hpp>
#include <oblo/reflection/concepts/random_access_container.hpp>
#include <oblo/reflection/concepts/ranged_type_erasure.hpp>
#include <oblo/reflection/reflection_registry.hpp>

#include <memory>
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
            requires std::is_class_v<T>
        class class_builder;

        template <typename T>
            requires std::is_enum_v<T>
        class enum_builder;

        template <typename T>
        class field_builder;

    public:
        explicit registrant(reflection_registry& registry) : m_impl{*registry.m_impl} {}

        template <typename T>
        class_builder<T> add_class();

        template <typename T>
        enum_builder<T> add_enum();

        template <typename T>
            requires std::is_fundamental_v<T>
        void add_fundamental();

    private:
        template <typename T>
        void auto_register();

        u32 add_type(const type_id& type, u32 size, u32 alignment, type_kind kind, bool* isNew = nullptr);
        u32 add_enum_type(
            const type_id& type, u32 size, u32 alignment, const type_id& underlying, bool* isNew = nullptr);

        template <typename T>
        u32 add_type(type_kind kind, bool* isNew = nullptr)
        {
            return add_type(get_type_id<T>(), sizeof(T), alignof(T), kind, isNew);
        }

        u32 add_field(u32 entityIndex, const type_id& type, cstring_view name, u32 offset);
        void* add_field_attribute(
            u32 entityIndex, u32 fieldIndex, const type_id& type, u32 size, u32 alignment, void (*destroy)(void*));
        void add_tag(u32 entityIndex, const type_id& type);
        void add_concept(
            u32 entityIndex, const type_id& type, u32 size, u32 alignment, const ranged_type_erasure& rte, void* src);
        void add_enumerator(u32 entityIndex, cstring_view name, std::span<const byte> value);

        template <typename C>
        void add_concept(u32 entityIndex, C value)
        {
            add_concept(entityIndex,
                get_type_id<concept_type<C>>(),
                sizeof(C),
                alignof(C),
                make_ranged_type_erasure<C>(),
                &value);
        }

        template <typename T, typename U>
        static u32 get_member_offset(U(T::*m))
        {
            alignas(T) std::byte buf[sizeof(T)];
            auto& t = *start_lifetime_as<T>(buf);

            u8* const bStructPtr = reinterpret_cast<u8*>(&t);
            u8* const bMemberPtr = reinterpret_cast<u8*>(&(t.*m));
            return u32(bMemberPtr - bStructPtr);
        }

        void make_array_type(u32 entityIndex, std::span<const usize> extents);

    private:
        reflection_registry_impl& m_impl;
    };

    template <typename T>
        requires std::is_class_v<T>
    class reflection_registry::registrant::class_builder
    {
    public:
        template <typename U>
        field_builder<T> add_field(U(T::*member), cstring_view name);

        template <typename U>
        class_builder& add_tag()
        {
            m_registrant.add_tag(m_entityIndex, get_type_id<tag_type<U>>());
            return *this;
        }

        template <typename C>
        class_builder& add_concept(C value)
        {
            m_registrant.add_concept(m_entityIndex, value);
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

    protected:
        registrant& m_registrant;
        u32 m_entityIndex;
    };

    template <typename T>
        requires std::is_enum_v<T>
    class reflection_registry::registrant::enum_builder
    {
    public:
        template <typename U>
        enum_builder& add_tag()
        {
            m_registrant.add_tag(m_entityIndex, get_type_id<tag_type<U>>());
            return *this;
        }

        enum_builder& add_enumerator(cstring_view name, T value)
        {
            m_registrant.add_enumerator(m_entityIndex, name, std::as_bytes(std::span{&value, 1}));
            return *this;
        }

    private:
        enum_builder(registrant& reg, u32 entityIndex) : m_registrant{reg}, m_entityIndex{entityIndex} {}

    private:
        friend class registrant;

    protected:
        registrant& m_registrant;
        u32 m_entityIndex;
    };

    template <typename T>
    class reflection_registry::registrant::field_builder : public class_builder<T>
    {
        using class_builder<T>::m_registrant;
        using class_builder<T>::m_entityIndex;

    public:
        friend class class_builder<T>;

        template <typename P, typename... Args>
        field_builder add_attribute(Args&&... args)
        {
            auto* ptr = m_registrant.add_field_attribute(m_entityIndex,
                m_fieldIndex,
                get_type_id<P>(),
                sizeof(T),
                alignof(T),
                [](void* p) { static_cast<T*>(p)->~T(); });

            new (ptr) T{std::forward<Args>(args)...};
            return *this;
        }

    private:
        field_builder(registrant& reg, u32 entityIndex, u32 fieldIndex) :
            class_builder<T>{reg, entityIndex}, m_fieldIndex{fieldIndex}
        {
        }

    private:
        u32 m_fieldIndex;
    };

    template <typename T>
    reflection_registry::registrant::class_builder<T> reflection_registry::registrant::add_class()
    {
        return {*this, add_type<T>(type_kind::class_kind)};
    }

    template <typename T>
    reflection_registry::registrant::enum_builder<T> reflection_registry::registrant::add_enum()
    {
        return {
            *this,
            add_enum_type(get_type_id<T>(), sizeof(T), alignof(T), get_type_id<std::underlying_type_t<T>>()),
        };
    }

    template <typename T>
        requires std::is_fundamental_v<T>
    void reflection_registry::registrant::add_fundamental()
    {
        add_type<T>(type_kind::fundamental_kind);
    }

    template <typename T>
    void reflection_registry::registrant::auto_register()
    {
        type_kind kind{};

        if constexpr (std::is_class_v<T>)
        {
            kind = type_kind::class_kind;
        }
        else if constexpr (std::is_bounded_array_v<T>)
        {
            kind = type_kind::array_kind;
        }
        else if constexpr (std::is_fundamental_v<T>)
        {
            kind = type_kind::fundamental_kind;
        }
        else if constexpr (std::is_enum_v<T>)
        {
            kind = type_kind::enum_kind;
        }

        bool isNew{};
        [[maybe_unused]] const u32 typeId = add_type<T>(kind, &isNew);

        if (isNew)
        {
            if constexpr (std::is_class_v<T>)
            {
                if constexpr (oblo::random_access_container<T>)
                {
                    class_builder<T>{*this, typeId}.add_concept(make_random_access_container<T>());
                    auto_register<typename T::value_type>();
                }

                if constexpr (requires { make_handle_concept<T>(); })
                {
                    class_builder<T>{*this, typeId}.add_concept(make_handle_concept<T>());
                }
            }
            else if constexpr (std::is_bounded_array_v<T>)
            {
                kind = type_kind::array_kind;
                auto_register<std::decay_t<std::remove_all_extents_t<T>>>();

                auto getExtents = []<u32... I>(std::integer_sequence<u32, I...>)
                {
                    struct result
                    {
                        usize extents[sizeof...(I)];
                    };

                    return result{{std::extent_v<T, I>...}};
                };

                constexpr auto r = getExtents(std::make_integer_sequence<u32, std::rank_v<T>>());
                make_array_type(typeId, r.extents);

                add_concept(typeId, make_random_access_container<T>());
            }
        }
    }

    template <typename T>
        requires std::is_class_v<T>
    template <typename U>
    inline reflection_registry::registrant::field_builder<T> reflection_registry::registrant::class_builder<
        T>::add_field(U(T::*member), cstring_view name)
    {
        const u32 offset = get_member_offset<T>(member);
        const u32 fieldIndex = m_registrant.add_field(m_entityIndex, get_type_id<U>(), name, offset);

        m_registrant.auto_register<U>();

        return field_builder<T>{m_registrant, m_entityIndex, fieldIndex};
    }
}