#pragma once

#include <oblo/core/types.hpp>

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
        bool m_drawAllBVHLevels{true};

        u32 m_bvhLevelToDraw{0};
        int m_gridSize{5};
        float m_density{.5f};
    };
}