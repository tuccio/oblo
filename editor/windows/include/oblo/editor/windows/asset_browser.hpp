#pragma once

#include <oblo/core/unique_ptr.hpp>

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
        struct impl;
        unique_ptr<impl> m_impl;
    };
}