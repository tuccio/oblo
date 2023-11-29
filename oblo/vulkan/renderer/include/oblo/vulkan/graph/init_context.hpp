#pragma once

#include <oblo/vulkan/renderer.hpp>

namespace oblo::vk
{
    class init_context
    {
    public:
        explicit init_context(renderer& renderer) : m_renderer{&renderer} {}

        render_pass_manager& get_render_pass_manager() const
        {
            return m_renderer->get_render_pass_manager();
        }

        string_interner& get_string_interner() const
        {
            return m_renderer->get_string_interner();
        }

    private:
        renderer* m_renderer;
    };
}