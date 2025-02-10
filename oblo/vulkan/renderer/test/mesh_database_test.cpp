#include <gtest/gtest.h>

#include <oblo/core/finally.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/vulkan/draw/mesh_database.hpp>
#include <oblo/vulkan/vulkan_context.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

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

            bool init()
            {
                // Load the runtime, which will be queried for required vulkan features
                auto& mm = module_manager::get();
                vulkan_engine_module* vkEngine = mm.load<vulkan_engine_module>();

                mm.finalize();

                auto& vkContext = vkEngine->get_vulkan_context();

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
                    .vertexBufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    .indexBufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    .meshBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    .tableVertexCount = 3 * N,
                    .tableIndexCount = 3 * N,
                });
            }

            void shutdown()
            {
                meshes.shutdown();
            }
        };
    }

    TEST(mesh_database_test, mesh_database_test)
    {
        module_manager mm;

        constexpr auto N{mesh_database_test::N};

        mesh_database_test app;

        ASSERT_TRUE(app.init());

        const auto cleanup = finally([&] { app.shutdown(); });

        mesh_handle handles[N * 4]{};

        for (u32 i = 0; i < N; ++i)
        {
            handles[0 + 4 * i] = app.meshes.create_mesh(
                (mesh_database_test::attributes::position | mesh_database_test::attributes::normal).data(),
                mesh_index_type::none,
                3,
                0,
                0);

            handles[1 + 4 * i] =
                app.meshes.create_mesh((mesh_database_test::attributes::position |
                                           mesh_database_test::attributes::normal | mesh_database_test::attributes::uv0)
                                           .data(),
                    mesh_index_type::none,
                    3,
                    0,
                    0);

            handles[2 + 4 * i] = app.meshes.create_mesh(
                (mesh_database_test::attributes::position | mesh_database_test::attributes::uv0).data(),
                mesh_index_type::u16,
                3,
                3,
                0);

            handles[3 + 4 * i] = app.meshes.create_mesh(flags{mesh_database_test::attributes::position}.data(),
                mesh_index_type::u32,
                3,
                3,
                0);
        }

        for (const auto h : handles)
        {
            ASSERT_TRUE(h);
        }
    }
}