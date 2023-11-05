#pragma once

#include <oblo/reflection/handles.hpp>
#include <oblo/reflection/reflection_data.hpp>

#include <oblo/core/type_id.hpp>

#include <memory>
#include <optional>
#include <span>

namespace oblo::reflection
{
    struct reflection_registry_impl;

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

        class_handle find_class(const type_id& type) const;

        std::span<const field_data> get_fields(class_handle classId) const;

        std::span<const type_id> find_by_tag(const type_id& tag) const;

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
};