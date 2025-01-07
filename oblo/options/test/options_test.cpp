#include <gtest/gtest.h>

#include <oblo/options/options_manager.hpp>

namespace oblo
{
    TEST(options_manager, basic)
    {
        options_manager options;

        constexpr uuid layerUuid = "01234567-89ab-cdef-0123-456789abcdef"_uuid;

        const options_layer_descriptor layers[] = {
            {.id = layerUuid},
        };

        options.init(layers);

        const auto myLayer = options.find_layer(layerUuid);
        ASSERT_TRUE(myLayer);
        ASSERT_NE(myLayer, options.get_default_layer());

        const auto myBool = options.register_option({
            .kind = property_kind::boolean,
            .name = "myBool",
            .category = "myCategory",
            .defaultValue = property_value_wrapper{false},
        });

        const auto myF32 = options.register_option({
            .kind = property_kind::f32,
            .name = "myFloat",
            .category = "anotherCategory",
            .defaultValue = property_value_wrapper{-32.f},
        });

        const auto myU32 = options.register_option({
            .kind = property_kind::u32,
            .name = "myU32",
            .category = "myCategory",
            .defaultValue = property_value_wrapper{42u},
        });

        {
            const auto myBoolVal = options.get_option_value(myLayer, myBool);
            ASSERT_TRUE(myBoolVal);
            ASSERT_EQ(myBoolVal->get_bool(), false);

            const auto myF32Val = options.get_option_value(myLayer, myF32);
            ASSERT_TRUE(myF32Val);
            ASSERT_EQ(myF32Val->get_f32(), -32.f);

            const auto myU32Val = options.get_option_value(myLayer, myU32);
            ASSERT_TRUE(myU32Val);
            ASSERT_EQ(myU32Val->get_u32(), 42u);
        }

        {
            ASSERT_TRUE(options.set_option_value(myLayer, myF32, property_value_wrapper{42.f}));
            ASSERT_TRUE(options.set_option_value(myLayer, myBool, property_value_wrapper{true}));
        }

        {
            const auto myBoolVal = options.get_option_value(myLayer, myBool);
            ASSERT_TRUE(myBoolVal);
            ASSERT_EQ(myBoolVal->get_bool(), true);

            const auto myF32Val = options.get_option_value(myLayer, myF32);
            ASSERT_TRUE(myF32Val);
            ASSERT_EQ(myF32Val->get_f32(), 42.f);

            const auto myU32Val = options.get_option_value(myLayer, myU32);
            ASSERT_TRUE(myU32Val);
            ASSERT_EQ(myU32Val->get_u32(), 42u);
        }

        {
            ASSERT_TRUE(options.clear_option_value(myLayer, myF32));

            const auto myF32Val = options.get_option_value(myLayer, myF32);
            ASSERT_TRUE(myF32Val);
            ASSERT_EQ(myF32Val->get_f32(), -32.f);
        }
    }
}