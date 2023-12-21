#include <gtest/gtest.h>

#include <oblo/core/finally.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/sandbox/sandbox_app.hpp>
#include <oblo/sandbox/sandbox_app_config.hpp>
#include <oblo/vulkan/draw/mesh_database.hpp>
#include <oblo/vulkan/staging_buffer.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk::test
{
    namespace
    {
        struct mesh_database_test
        {
            static constexpr u32 N{2};

            enum class attributes
            {
                position,
                normal,
                uv0,
                enum_max
            };

            mesh_database meshes;

            bool init(const vk::sandbox_init_context& ctx)
            {
                auto& vkContext = *ctx.vkContext;

                mesh_attribute_description attributes[] = {
                    {
                        .name = h32<string>{u32(attributes::position)},
                        .elementSize = sizeof(vec3),
                    },
                    {
                        .name = h32<string>{u32(attributes::normal)},
                        .elementSize = sizeof(vec3),
                    },
                    {
                        .name = h32<string>{u32(attributes::uv0)},
                        .elementSize = sizeof(vec2),
                    },
                };

                return meshes.init({
                    .allocator = vkContext.get_allocator(),
                    .resourceManager = vkContext.get_resource_manager(),
                    .attributes = attributes,
                    .tableVertexCount = 3 * N,
                    .tableIndexCount = 3 * N,
                });
            }

            void shutdown(const vk::sandbox_shutdown_context&)
            {
                meshes.shutdown();
            }

            void update(const vk::sandbox_render_context&) {}

            void update_imgui(const vk::sandbox_update_imgui_context&) {}
        };
    }

    TEST(mesh_database_test, mesh_database_test)
    {
        constexpr auto N{mesh_database_test::N};

        sandbox_app<mesh_database_test> app;

        app.set_config(sandbox_app_config{
            .vkUseValidationLayers = true,
        });

        ASSERT_TRUE(app.init());

        mesh_handle handles[N * 4]{};

        for (u32 i = 0; i < N; ++i)
        {
            handles[0 + 4 * i] = app.meshes.create_mesh(
                (flags{mesh_database_test::attributes::position} | mesh_database_test::attributes::normal).data(),
                mesh_index_type::none,
                3,
                0);

            handles[1 + 4 * i] =
                app.meshes.create_mesh((flags{mesh_database_test::attributes::position} |
                                           mesh_database_test::attributes::normal | mesh_database_test::attributes::uv0)
                                           .data(),
                    mesh_index_type::none,
                    3,
                    0);

            handles[2 + 4 * i] = app.meshes.create_mesh(
                (flags{mesh_database_test::attributes::position} | mesh_database_test::attributes::uv0).data(),
                mesh_index_type::u16,
                3,
                3);

            handles[3 + 4 * i] = app.meshes.create_mesh(flags{mesh_database_test::attributes::position}.data(),
                mesh_index_type::u32,
                3,
                3);
        }

        for (const auto h : handles)
        {
            ASSERT_TRUE(h);
        }
    }
}