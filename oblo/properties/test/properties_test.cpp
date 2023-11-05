#include <gtest/gtest.h>

#include <oblo/core/overload.hpp>
#include <oblo/core/types.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/visit.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/registration/registrant.hpp>

namespace oblo
{
    namespace
    {
        struct flat_pod
        {
            i8 i8Field;
            i16 i16Field;
            i32 i32Field;
            i64 i64Field;

            u8 u8Field;
            u16 u16Field;
            u32 u32Field;
            u64 u64Field;

            f32 f32Field;
            f64 f64Field;

            bool booleanField;
        };

        struct three_ints
        {
            u32 x, y, z;
        };

        struct bool_and_int
        {
            bool b;
            i32 i;
        };

        struct both
        {
            three_ints ti;
            bool_and_int bi;
        };

        struct hierarchical
        {
            bool_and_int a;
            i8 b;
            three_ints c;
            both d;
        };

        const property* find_property(const property_tree& tree, std::initializer_list<const std::string_view> chain)
        {
            const property* res{};

            auto it = chain.begin();

            visit(tree,
                overload{
                    [&it](const property_node& node, const property_node_start)
                    {
                        if (node.name == *it)
                        {
                            ++it;
                            return property_visit_result::recurse;
                        }

                        return property_visit_result::sibling;
                    },
                    [](const property_node&, const property_node_finish) {},
                    [&it, &chain, &res](const property& property)
                    {
                        if (property.name == *it)
                        {
                            if (++it == chain.end())
                            {
                                res = &property;
                            }

                            return property_visit_result::terminate;
                        }

                        return property_visit_result::sibling;
                    },
                });

            return res;
        }

    }

    TEST(properties, hierarchical)
    {
        reflection::reflection_registry reflection;

        auto registrant = reflection.get_registrant();

        registrant.add_class<hierarchical>()
            .add_field(&hierarchical::a, "a")
            .add_field(&hierarchical::b, "b")
            .add_field(&hierarchical::c, "c")
            .add_field(&hierarchical::d, "d");

        registrant.add_class<both>().add_field(&both::ti, "ti").add_field(&both::bi, "bi");

        registrant.add_class<bool_and_int>().add_field(&bool_and_int::b, "b").add_field(&bool_and_int::i, "i");

        registrant.add_class<three_ints>()
            .add_field(&three_ints::x, "x")
            .add_field(&three_ints::y, "y")
            .add_field(&three_ints::z, "z");

        property_registry properties;

        properties.init(reflection);
        auto* const tree = properties.build_from_reflection(get_type_id<hierarchical>());

        ASSERT_TRUE(tree != nullptr);

        ASSERT_EQ(tree->properties.size(), 11);

        const auto* ab = find_property(*tree, {"a", "b"});
        ASSERT_TRUE(ab);

        ASSERT_EQ(ab->kind, property_kind::boolean);
    }

    TEST(properties, flat_pod)
    {
        reflection::reflection_registry reflection;

        auto registrant = reflection.get_registrant();

        registrant.add_class<flat_pod>()
            .add_field(&flat_pod::i8Field, "i8Field")
            .add_field(&flat_pod::i16Field, "i16Field")
            .add_field(&flat_pod::i32Field, "i32Field")
            .add_field(&flat_pod::i64Field, "i64Field")

            .add_field(&flat_pod::u8Field, "u8Field")
            .add_field(&flat_pod::u16Field, "u16Field")
            .add_field(&flat_pod::u32Field, "u32Field")
            .add_field(&flat_pod::u64Field, "u64Field")

            .add_field(&flat_pod::f32Field, "f32Field")
            .add_field(&flat_pod::f64Field, "f64Field")

            .add_field(&flat_pod::booleanField, "booleanField");

        property_registry properties;

        properties.init(reflection);
        auto* const podTree = properties.build_from_reflection(get_type_id<flat_pod>());

        ASSERT_TRUE(podTree != nullptr);

        ASSERT_EQ(podTree->properties.size(), 11);
    }
}