#pragma once

namespace oblo
{
    class options_manager;
}

namespace oblo::editor
{
    struct window_update_context;

    class options_editor final
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context&);

    private:
        options_manager* m_options{};
    };
}