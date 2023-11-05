#pragma once

#include <oblo/reflection/handles.hpp>
#include <oblo/reflection/reflection_data.hpp>

#include <oblo/core/type_id.hpp>

#include <memory>
#include <optional>
#include <span>
#include <vector>

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
        class_handle find_class() const;

        type_handle find_type(const type_id& type) const;
        class_handle find_class(const type_id& type) const;

        type_data get_type_data(type_handle typeId) const;

        std::span<const field_data> get_fields(class_handle classId) const;

        template <typename T>
        void find_by_tag(std::vector<type_handle>& types) const;

        template <typename T>
        std::optional<T> find_concept(type_handle typeId) const;

    private:
        void find_by_tag(const type_id& tag, std::vector<type_handle>& types) const;
        const void* find_concept(type_handle typeId, const type_id& type) const;

    private:
        friend class registrant;

    private:
        std::unique_ptr<reflection_registry_impl> m_impl;
    };

    template <typename T>
    class_handle reflection_registry::find_class() const
    {
        return find_class(get_type_id<T>());
    }

    template <typename T>
    void reflection_registry::find_by_tag(std::vector<type_handle>& types) const
    {
        find_by_tag(get_type_id<tag_type<T>>(), types);
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
};