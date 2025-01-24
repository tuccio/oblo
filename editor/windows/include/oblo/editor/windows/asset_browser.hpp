#pragma once

#include <oblo/core/deque.hpp>
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
        asset_browser();
        ~asset_browser();

        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        void reset_path();

        void populate_create_menu();
        void draw_create_menu();

    private:
        struct create_menu_item;

    private:
        asset_registry* m_registry{};
        string_builder m_path;
        string_builder m_current;
        uuid m_expandedAsset{};
        dynamic_array<string> m_breadcrumbs;
        deque<create_menu_item> m_createMenu;
    };
}