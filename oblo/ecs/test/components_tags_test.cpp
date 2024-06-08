#include <gtest/gtest.h>

#include <oblo/core/iterator/zip_range.hpp>
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

    TEST(components_tags_test, remove_component)
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

        const auto e = reg.create<mock_name_component, mock_audio_source_component>();

        {
            auto&& [name, audio] = reg.get<mock_name_component, mock_audio_source_component>(e);

            name.name = 'A';
            audio.resourceId = 42u;
        }

        auto checkEntity = [&reg, e]
        {
            const auto entityArch = reg.get_archetype_storage(e);

            for (const auto arch : reg.get_archetypes())
            {
                if (entityArch == arch)
                {
                    ASSERT_EQ(get_entities_count(arch), 1);
                    ASSERT_EQ(get_entities_count_in_chunk(arch, 0), 1);
                }
                else
                {
                    ASSERT_EQ(get_entities_count(arch), 0);
                    ASSERT_EQ(get_entities_count_in_chunk(arch, 0), 0);
                }
            }
        };

        ASSERT_EQ(reg.get_archetypes().size(), 1);
        checkEntity();

        // Nothing should happen
        reg.remove<mock_sprite_component>(e);

        {
            auto&& [name, audio] = reg.get<mock_name_component, mock_audio_source_component>(e);

            ASSERT_EQ(name.name, 'A');
            ASSERT_EQ(audio.resourceId, 42u);
        }

        ASSERT_EQ(reg.get_archetypes().size(), 1);
        checkEntity();

        // This should move to a different archetype
        reg.remove<mock_audio_source_component>(e);

        {
            auto&& name = reg.get<mock_name_component>(e);

            ASSERT_EQ(name.name, 'A');
        }

        ASSERT_EQ(reg.get_archetypes().size(), 2);
        checkEntity();
    }
}