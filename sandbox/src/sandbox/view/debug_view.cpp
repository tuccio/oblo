#include <sandbox/view/debug_view.hpp>

#include <oblo/math/line.hpp>
#include <oblo/math/triangle.hpp>
#include <oblo/rendering/material.hpp>
#include <oblo/rendering/raytracer.hpp>
#include <sandbox/draw/debug_renderer.hpp>
#include <sandbox/import/scene_importer.hpp>
#include <sandbox/state/sandbox_state.hpp>

#include <imgui.h>
#include <random>

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

        [[maybe_unused]] void init_cube(sandbox_state& state)
        {
            triangle_container triangles;
            triangles.add(s_cube);

            state.raytracer->clear();
            u32 mesh = state.raytracer->add_mesh(std::move(triangles));
            u32 material = state.raytracer->add_material({vec3{1.f, 0.f, 0.f}});
            state.raytracer->add_instance({mesh, material});
            state.raytracer->rebuild_tlas();
        }

        void init_cubes_scene(sandbox_state& state, u32 gridSize, float density)
        {
            std::mt19937 rng{42};
            std::uniform_real_distribution<float> dist{0.f, 1.f};

            state.raytracer->clear();

            constexpr auto cellSize = 1.f;
            const auto baseOffset = -(cellSize * gridSize) * .5f;

            vec3 colors[] =
                {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 0.f}, {0.f, 1.f, 1.f}, {1.f, 0.f, 1.f}};

            for (u32 i = 0; i < gridSize; ++i)
            {
                vec3 offset;
                offset.y = baseOffset + cellSize * i;
                offset.z = 0.f;

                for (u32 j = 0; j < gridSize; ++j)
                {
                    offset.x = baseOffset + cellSize * j;

                    if (dist(rng) < density)
                    {
                        constexpr auto numTriangles = std::size(s_cube);
                        triangle cube[numTriangles];

                        for (std::size_t triangleIndex = 0; triangleIndex < numTriangles; ++triangleIndex)
                        {
                            auto& src = s_cube[triangleIndex];
                            auto& dst = cube[triangleIndex];

                            for (u32 vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
                            {
                                dst.v[vertexIndex] = src.v[vertexIndex] + offset;
                            }
                        }

                        triangle_container cubeTriangles;
                        cubeTriangles.add(cube);

                        u32 mesh = state.raytracer->add_mesh(std::move(cubeTriangles));
                        u32 material = state.raytracer->add_material({colors[mesh % std::size(colors)]});

                        state.raytracer->add_instance({mesh, material});
                    }
                }
            }

            state.raytracer->rebuild_tlas();
        }

        void import_last_scene(sandbox_state& state)
        {
            state.raytracer->clear();

            if (state.latestImportedScene.empty())
            {
                return;
            }

            scene_importer importer;
            importer.import(state, state.latestImportedScene);
        }
    }

    void debug_view::update(sandbox_state& state)
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Scene"))
            {
                if (ImGui::MenuItem("Import last scene"))
                {
                    import_last_scene(state);
                }

                if (ImGui::MenuItem("Load debug scene"))
                {
                    init_cubes_scene(state, m_gridSize, m_density);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Camera"))
                {
                    m_cameraWindow = true;
                }

                if (ImGui::MenuItem("Scene"))
                {
                    m_sceneWindow = true;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        if (m_cameraWindow)
        {
            ImGui::Begin("Camera", &m_cameraWindow);

            ImGui::DragFloat("X", &state.camera.position.x, .2f);
            ImGui::DragFloat("Y", &state.camera.position.y, .2f);
            ImGui::DragFloat("Z", &state.camera.position.z, .2f);

            ImGui::SliderFloat("Near", &state.camera.near, 0.1f, 1.f);
            ImGui::SliderFloat("Far", &state.camera.far, 10.f, 1000.f);

            ImGui::End();
        }

        if (m_sceneWindow)
        {
            ImGui::Begin("Debug scene", &m_cameraWindow);

            ImGui::SliderInt("Grid size", &m_gridSize, 1, 64);
            ImGui::SliderFloat("Density", &m_density, 0.f, 1.f);

            if (ImGui::Button("Apply"))
            {
                init_cubes_scene(state, narrow_cast<u32>(m_gridSize), m_density);
            }

            ImGui::Checkbox("Rasterize", &state.renderRasterized);
            ImGui::Checkbox("Draw BVH", &m_drawBVH);
            ImGui::Checkbox("Draw all BVH levels", &m_drawAllBVHLevels);

            if (!m_drawAllBVHLevels)
            {
                ImGui::DragScalar("BVH level to draw", ImGuiDataType_U32, &m_bvhLevelToDraw, .2f);
            }

            {
                const auto& metrics = state.raytracerState->get_metrics();
                ImGui::Text("Objects count: %d", metrics.numObjects);
                ImGui::Text("Triangles count: %d", metrics.numTriangles);
                ImGui::Text("Primary rays count: %d", metrics.numPrimaryRays);
                ImGui::Text("Objects tests count: %d", metrics.numTestedObjects);
                ImGui::Text("Triangles tests count: %d", metrics.numTestedTriangles);
            }

            ImGui::End();
        }

        if (state.renderRasterized && m_drawAllBVHLevels)
        {
            vec3 colors[] =
                {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 0.f}, {0.f, 1.f, 1.f}, {1.f, 0.f, 1.f}};

            u32 colorIndex{0};

            for (const auto& mesh : state.raytracer->get_meshes())
            {
                state.debugRenderer->draw_triangles(mesh.get_triangles(), colors[colorIndex]);
                colorIndex = (colorIndex + 1) % std::size(colors);
            }
        }

        if (m_drawBVH)
        {
            state.raytracer->get_tlas().visit(
                [this, &state](u32 depth, const aabb& bounds, u32 /*offset*/, u32 /*numPrimitives*/)
                {
                    if (!m_drawAllBVHLevels && depth != m_bvhLevelToDraw)
                    {
                        return;
                    }

                    const auto& [min, max] = bounds;

                    const line lines[] = {// front face
                                          {{{min.x, min.y, min.z}, {max.x, min.y, min.z}}},
                                          {{{min.x, max.y, min.z}, {max.x, max.y, min.z}}},
                                          {{{min.x, max.y, min.z}, {min.x, min.y, min.z}}},
                                          {{{max.x, max.y, min.z}, {max.x, min.y, min.z}}},
                                          // back face
                                          {{{min.x, min.y, max.z}, {max.x, min.y, max.z}}},
                                          {{{min.x, max.y, max.z}, {max.x, max.y, max.z}}},
                                          {{{min.x, max.y, max.z}, {min.x, min.y, max.z}}},
                                          {{{max.x, max.y, max.z}, {max.x, min.y, max.z}}},
                                          // connect the two
                                          {{{min.x, min.y, min.z}, {min.x, min.y, max.z}}},
                                          {{{max.x, min.y, min.z}, {max.x, min.y, max.z}}},
                                          {{{max.x, max.y, min.z}, {max.x, max.y, max.z}}},
                                          {{{min.x, max.y, min.z}, {min.x, max.y, max.z}}}};

                    state.debugRenderer->draw_lines(lines, {0.f, 1.f, 0.f});
                });
        }
    }
}