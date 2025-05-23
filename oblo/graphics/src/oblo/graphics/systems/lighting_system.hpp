#pragma once

#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/ecs/forward.hpp>
#include <oblo/ecs/utility/entity_map.hpp>
#include <oblo/graphics/systems/graphics_options.hpp>
#include <oblo/options/option_proxy.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo::vk
{
    class renderer;
}

namespace oblo
{
    struct option;

    class options_manager;
    class scene_renderer;

    class lighting_system
    {
    public:
        lighting_system();
        lighting_system(const lighting_system&) = delete;
        lighting_system(lighting_system&&) noexcept = delete;

        ~lighting_system();

        lighting_system& operator=(const lighting_system&) = delete;
        lighting_system& operator=(lighting_system&&) noexcept = delete;

        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        struct shadow_directional;

    private:
        scene_renderer* m_sceneRenderer{};
        const options_manager* m_optionsManager{};
        vk::frame_graph_template m_rtShadows;
        ecs::entity_map<shadow_directional> m_shadows;
        option_proxy_struct<surfels_gi_options> m_giOptions;
        option_proxy_struct<rtao_options> m_rtaoOptions;
    };
};
