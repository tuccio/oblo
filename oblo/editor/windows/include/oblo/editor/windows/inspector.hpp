#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/unique_ptr.hpp>

namespace oblo
{
    class property_registry;
}

namespace oblo::reflection
{
    class reflection_registry;
}

namespace oblo::editor
{
    class component_factory;
    class editor_world;

    struct window_update_context;

    class inspector final
    {
    public:
        inspector();
        inspector(const inspector&) = delete;
        inspector(inspector&&) noexcept = delete;
        ~inspector();

        inspector& operator=(const inspector&) = delete;
        inspector& operator=(inspector&&) noexcept = delete;

        bool init(const window_update_context&);
        bool update(const window_update_context&);

    public:
        struct data_inspector_ctx;

    private:
        const property_registry* m_propertyRegistry{};
        const reflection::reflection_registry* m_reflection{};
        const editor_world* m_editorWorld{};
        const component_factory* m_factory{};
        unique_ptr<data_inspector_ctx> m_ctx;
    };
}