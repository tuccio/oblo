#include <sandbox/view/debug_view.hpp>

#include <oblo/math/triangle.hpp>
#include <sandbox/draw/debug_renderer.hpp>
#include <sandbox/sandbox_state.hpp>

#include <imgui.h>

namespace oblo
{
    namespace
    {
        constexpr triangle s_cube[] = {
            // front face
            {{{-.5f, -.5f, -.5f}, {-.5f, .5f, -.5f}, {.5f, .5f, -.5f}}},
            {{{.5f, -.5f, -.5f}, {-.5f, -.5f, -.5f}, {.5f, .5f, -.5f}}},
            // back face
            {{{-.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}}},
            {{{.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, -.5f, .5f}}},
            // left face
            {{{.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}}},
            {{{.5f, -.5f, -.5f}, {.5f, .5f, .5f}, {.5f, -.5f, .5f}}},
            // right face
            {{{-.5f, -.5f, -.5f}, {-.5f, .5f, .5f}, {-.5f, .5f, -.5f}}},
            {{{-.5f, -.5f, -.5f}, {-.5f, -.5f, .5f}, {-.5f, .5f, .5f}}},
            // top face
            {{{-.5f, .5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, -.5f}}},
            {{{.5f, .5f, .5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}}},
            // bottom face
            {{{-.5f, -.5f, .5f}, {-.5f, -.5f, -.5f}, {.5f, -.5f, .5f}}},
            {{{.5f, -.5f, .5f}, {-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}}},
        };

        void init_cube(sandbox_state& state)
        {
            state.triangles.clear();
            state.triangles.add(s_cube);
            state.bvh.build(state.triangles);
        }

        void render_rasterized(sandbox_state& state)
        {
            state.debugRenderer->draw(state.triangles.get_triangles(), vec3{1.f, 0.f, 0.f});
        }
    }

    void debug_view::update(sandbox_state& state)
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Debug"))
            {
                if (ImGui::MenuItem("Load cube"))
                {
                    init_cube(state);
                }

                if (ImGui::BeginMenu("Edit"))
                {
                    if (ImGui::MenuItem("Camera"))
                    {
                        m_cameraWindow = true;
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        if (m_cameraWindow)
        {
            ImGui::Begin("Camera", &m_cameraWindow);

            ImGui::SliderFloat("X", &state.camera.position.x, -10.f, 10.f);
            ImGui::SliderFloat("Y", &state.camera.position.y, -10.f, 10.f);
            ImGui::SliderFloat("Z", &state.camera.position.z, -10.f, 10.f);

            ImGui::SliderFloat("Near", &state.camera.near, 0.1f, 1.f);
            ImGui::SliderFloat("Far", &state.camera.far, 10.f, 1000.f);

            ImGui::End();
        }

        if (state.renderRasterized)
        {
            render_rasterized(state);
        }
    }
}