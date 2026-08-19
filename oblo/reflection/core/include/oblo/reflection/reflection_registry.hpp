#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/reflection/handles.hpp>
#include <oblo/reflection/reflection_data.hpp>

#include <optional>
#include <span>

namespace oblo::reflection
{
    template <typename T>
    struct concept_type;

    template <typename T>
    struct tag_type;

    struct reflection_registry_impl;

    struct type_info;

    class reflection_registry
    {
    public:
        class registrant;

    public:
        reflection_registry();
        reflection_registry(const reflection_registry&) = delete;
        reflection_registry(reflection_registry&&) noexcept;

        reflection_registry& operator=(const reflection_registry&) = delete;
        reflection_registry& operator=(reflection_registry&&) noexcept;

        ~reflection_registry();

        registrant get_registrant();

        template <typename T>
        type_handle find_type() const;

        template <typename T>
        class_handle find_class() const;

        template <typename T>
        enum_handle find_enum() const;

        type_handle find_type(const type_id& type) const;
        class_handle find_class(const type_id& type) const;
        enum_handle find_enum(const type_id& type) const;

        type_data get_type_data(type_handle typeId) const;
        class_handle try_get_class(type_handle typeId) const;
        enum_handle try_get_enum(type_handle typeId) const;

        std::span<const field_data> get_fields(class_handle classId) const;

        std::span<const function_handle> get_functions(class_handle classId) const;
        function_data get_function_data(function_handle functionId) const;

        std::span<const cstring_view> get_enumerator_names(enum_handle enumId) const;
        std::span<const byte> get_enumerator_values(enum_handle enumId) const;
        type_id get_underlying_type(enum_handle enumId) const;

        template <typename T>
        bool has_tag(type_handle typeId) const;

        template <typename T>
        bool has_tag(function_handle function) const;

        template <typename T>
        void find_by_tag(deque<type_handle>& types) const;

        template <typename T>
        void find_by_concept(deque<type_handle>& types) const;

        template <typename T>
        void find_by_tag(deque<function_handle>& functions) const;

        template <typename T>
        void find_by_concept(deque<function_handle>& functions) const;

        template <typename T>
        std::optional<T> find_concept(type_handle typeId) const;

        bool is_fundamental(type_handle typeId) const;

    private:
        template <typename T>
        bool has_tag(const type_id& tag, T type) const;

        template <typename T>
        void find_by_tag(const type_id& tag, deque<T>& handles) const;

        template <typename T>
        void find_by_concept(const type_id& type, deque<T>& handles) const;

        const void* find_concept(type_handle typeId, const type_id& type) const;

    private:
        friend class registrant;

    private:
        unique_ptr<reflection_registry_impl> m_impl;
    };

    template <typename T>
    type_handle reflection_registry::find_type() const
    {
        return find_type(get_type_id<T>());
    }

    template <typename T>
    class_handle reflection_registry::find_class() const
    {
        return find_class(get_type_id<T>());
    }

    template <typename T>
    enum_handle reflection_registry::find_enum() const
    {
        return find_enum(get_type_id<T>());
    }

    template <typename T>
    void reflection_registry::find_by_tag(deque<type_handle>& types) const
    {
        find_by_tag(get_type_id<tag_type<T>>(), types);
    }

    template <typename T>
    void reflection_registry::find_by_concept(deque<type_handle>& types) const
    {
        find_by_concept(get_type_id<concept_type<T>>(), types);
    }

    template <typename T>
    void reflection_registry::find_by_tag(deque<function_handle>& functions) const
    {
        find_by_tag(get_type_id<tag_type<T>>(), functions);
    }

    template <typename T>
    void reflection_registry::find_by_concept(deque<function_handle>& functions) const
    {
        find_by_concept(get_type_id<concept_type<T>>(), functions);
    }

    template <typename T>
    std::optional<T> reflection_registry::find_concept(type_handle typeId) const
    {
        std::optional<T> res;
        auto* const ptr = find_concept(typeId, get_type_id<concept_type<T>>());

        if (ptr)
        {
            res.emplace(*static_cast<const T*>(ptr));
        }

        return res;
    }

    template <typename T>
    bool reflection_registry::has_tag(type_handle type) const
    {
        return has_tag(get_type_id<tag_type<T>>(), type);
    }

    template <typename T>
    bool reflection_registry::has_tag(function_handle function) const
    {
        return has_tag(get_type_id<tag_type<T>>(), function);
    }
};