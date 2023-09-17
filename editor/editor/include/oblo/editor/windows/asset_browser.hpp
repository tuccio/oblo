#pragma once

#include <filesystem>
#include <vector>

namespace oblo::asset
{
    class registry;
}

namespace oblo::editor
{
    class asset_browser final
    {
    public:
        asset_browser() = delete;
        asset_browser(const asset_browser&) = delete;
        asset_browser(asset_browser&&) noexcept = delete;
        explicit asset_browser(asset::registry& registry);

        asset_browser& operator=(const asset_browser&) = delete;
        asset_browser& operator=(asset_browser&&) noexcept = delete;

        bool update();

    private:
        void reset_path();

    private:
        asset::registry* m_registry{};
        std::filesystem::path m_path;
        std::filesystem::path m_current;
        std::vector<std::filesystem::path> m_breadcrumbs;
    };
}