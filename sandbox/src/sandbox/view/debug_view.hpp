#pragma once

namespace oblo
{
    struct sandbox_state;

    class debug_view
    {
    public:
        void update(sandbox_state& state);

    private:
        bool m_cameraWindow{false};
    };
}