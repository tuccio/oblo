#include <gtest/gtest.h>

#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/math/vec2.hpp>

#include <array>

namespace oblo::ecs
{
    namespace
    {
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

    TEST(components_tags_test, multiple_archetypes)
    {
        type_registry typeRegistry;

        {
            const component_type name = register_type<mock_name_component>(typeRegistry);
            const component_type sprite = register_type<mock_sprite_component>(typeRegistry);
            const component_type audio = register_type<mock_audio_source_component>(typeRegistry);
            const tag_type selected = register_type<mock_selected_tag>(typeRegistry);
            const tag_type disabled = register_type<mock_disabled_tag>(typeRegistry);

            ASSERT_TRUE(sprite);
            ASSERT_TRUE(audio);
            ASSERT_TRUE(name);
            ASSERT_TRUE(selected);
            ASSERT_TRUE(disabled);
        }

        entity_registry reg{&typeRegistry};

        std::array<entity, 255> nameToEntityMap = {};

        {
            const auto A = reg.create<mock_sprite_component, mock_name_component>();
            ASSERT_TRUE(A);

            auto&& [spriteA, nameA] = reg.get<mock_sprite_component, mock_name_component>(A);
            spriteA.resourceId = ~u32('A');
            nameA.name = 'A';

            nameToEntityMap['A'] = A;
        }

        {
            const auto B = reg.create<mock_sprite_component, mock_name_component, mock_disabled_tag>();
            ASSERT_TRUE(B);

            auto&& [spriteB, nameB] = reg.get<mock_sprite_component, mock_name_component>(B);
            spriteB.resourceId = ~u32('B');
            nameB.name = 'B';

            nameToEntityMap['B'] = B;
        }

        {
            const auto C = reg.create<mock_audio_source_component, mock_name_component, mock_disabled_tag>();
            ASSERT_TRUE(C);

            auto&& [audioC, nameC] = reg.get<mock_audio_source_component, mock_name_component>(C);
            audioC.resourceId = ~u32('C');
            nameC.name = 'C';

            nameToEntityMap['C'] = C;
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

            nameToEntityMap['D'] = D;
        }

        {
            std::array<bool, 255> entitiesSet = {};
            u32 count{};

            for (auto&& [entitiesRange, nameRange, spriteRange] :
                reg.range<mock_name_component, mock_sprite_component>())
            {
                for (auto&& [e, name, sprite] : zip_range(entitiesRange, nameRange, spriteRange))
                {
                    ASSERT_EQ(e, nameToEntityMap[name.name]);
                    ASSERT_EQ(~u32(name.name), sprite.resourceId);
                    entitiesSet[name.name] = true;
                    ++count;
                }
            }

            ASSERT_EQ(count, 3);
            ASSERT_TRUE(entitiesSet['A']);
            ASSERT_TRUE(entitiesSet['B']);
            ASSERT_FALSE(entitiesSet['C']);
            ASSERT_TRUE(entitiesSet['D']);
        }
    }
}