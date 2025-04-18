#include <gtest/gtest.h>

#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/tag_type_desc.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/math/vec2.hpp>

#include <array>

namespace oblo::ecs
{
    namespace
    {
        template <typename T>
        struct dtor_counter
        {
            inline static u32 value{};
        };

        template <typename T>
        auto register_type_count_dtor(type_registry& registry)
        {
            if constexpr (is_tag_v<T>)
            {
                return registry.register_tag(make_tag_type_desc<T>());
            }
            else
            {
                dtor_counter<T>::value = 0;

                ecs::component_type_desc desc = make_component_type_desc<T>();

                desc.destroy = [](void* dst, usize count)
                {
                    T* outIt = static_cast<T*>(dst);
                    T* const end = outIt + count;

                    for (; outIt != end; ++outIt)
                    {
                        outIt->~T();
                        ++dtor_counter<T>::value;
                    }
                };

                return registry.register_component(desc);
            }
        }

        struct mock_sprite_component
        {
            u32 resourceId;
        };

        struct mock_audio_source_component
        {
            u32 resourceId;
        };

        struct mock_name_component
        {
            char name;
        };

        struct mock_selected_tag
        {
        };

        struct mock_disabled_tag
        {
        };
    }

    TEST(entity_registry_destroy_all, multiple_archetypes)
    {
        type_registry typeRegistry;

        {
            const component_type name = register_type_count_dtor<mock_name_component>(typeRegistry);
            const component_type sprite = register_type_count_dtor<mock_sprite_component>(typeRegistry);
            const component_type audio = register_type_count_dtor<mock_audio_source_component>(typeRegistry);
            const tag_type selected = register_type_count_dtor<mock_selected_tag>(typeRegistry);
            const tag_type disabled = register_type_count_dtor<mock_disabled_tag>(typeRegistry);

            ASSERT_TRUE(sprite);
            ASSERT_TRUE(audio);
            ASSERT_TRUE(name);
            ASSERT_TRUE(selected);
            ASSERT_TRUE(disabled);
        }

        entity_registry reg{&typeRegistry};

        {
            const auto A = reg.create<mock_sprite_component, mock_name_component>();
            ASSERT_TRUE(A);

            auto&& [spriteA, nameA] = reg.get<mock_sprite_component, mock_name_component>(A);
            spriteA.resourceId = ~u32('A');
            nameA.name = 'A';
        }

        {
            const auto B = reg.create<mock_sprite_component, mock_name_component, mock_disabled_tag>();
            ASSERT_TRUE(B);

            auto&& [spriteB, nameB] = reg.get<mock_sprite_component, mock_name_component>(B);
            spriteB.resourceId = ~u32('B');
            nameB.name = 'B';
        }

        {
            const auto C = reg.create<mock_audio_source_component, mock_name_component, mock_disabled_tag>();
            ASSERT_TRUE(C);

            auto&& [audioC, nameC] = reg.get<mock_audio_source_component, mock_name_component>(C);
            audioC.resourceId = ~u32('C');
            nameC.name = 'C';
        }

        {
            const auto D = reg.create<mock_audio_source_component,
                mock_sprite_component,
                mock_name_component,
                mock_selected_tag>();
            ASSERT_TRUE(D);

            auto&& [audioD, spriteD, nameD] =
                reg.get<mock_audio_source_component, mock_sprite_component, mock_name_component>(D);
            spriteD.resourceId = ~u32('D');
            nameD.name = 'D';
        }

        ASSERT_GT(reg.get_archetypes().size(), 0);

        for (const auto& arch : reg.get_archetypes())
        {
            ASSERT_GT(ecs::get_entities_count(arch), 0);
        }

        ASSERT_EQ(dtor_counter<mock_name_component>::value, 0);
        ASSERT_EQ(dtor_counter<mock_sprite_component>::value, 0);
        ASSERT_EQ(dtor_counter<mock_audio_source_component>::value, 0);

        reg.destroy_all();

        ASSERT_EQ(dtor_counter<mock_name_component>::value, 4);
        ASSERT_EQ(dtor_counter<mock_sprite_component>::value, 3);
        ASSERT_EQ(dtor_counter<mock_audio_source_component>::value, 2);

        for (const auto& arch : reg.get_archetypes())
        {
            ASSERT_EQ(ecs::get_entities_count(arch), 0);
        }
    }
}