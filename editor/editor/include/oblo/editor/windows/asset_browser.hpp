#pragma once

#include <filesystem>
#include <vector>

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
        std::filesystem::path m_path;
        std::filesystem::path m_current;
        std::vector<std::filesystem::path> m_breadcrumbs;
    };
}