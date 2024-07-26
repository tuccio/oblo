#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    class asset_registry;
}

namespace oblo::editor
{
    struct window_update_context;

    class asset_browser final
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        void reset_path();

    private:
        asset_registry* m_registry{};
        string_builder m_path;
        string_builder m_current;
        uuid m_expandedAsset{};
        dynamic_array<string> m_breadcrumbs;
    };
}