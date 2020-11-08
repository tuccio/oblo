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
        bool m_sceneWindow{false};

        bool m_drawBVH{true};

        int m_gridSize{5};
        float m_density{.5f};
    };
}