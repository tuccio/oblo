#include <gtest/gtest.h>

#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/options/options_manager.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>

namespace oblo
{
    namespace
    {
        class options_test : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                const options_layer_descriptor layers[] = {
                    {.id = layerUuid},
                };

                options.init(layers);

                myLayer = options.find_layer(layerUuid);
                ASSERT_TRUE(myLayer);
                ASSERT_NE(myLayer, options.get_default_layer());

                myBool = options.register_option({
                    .kind = property_kind::boolean,
                    .id = "04c34e08-e990-4c3b-bcd6-92345ad17ad5"_uuid,
                    .name = "myBool",
                    .category = "myCategory",
                    .defaultValue = property_value_wrapper{false},
                });

                myF32 = options.register_option({
                    .kind = property_kind::f32,
                    .id = "e53eae3d-a807-4d32-b95e-bfcc66dd1e82"_uuid,
                    .name = "myFloat",
                    .category = "anotherCategory",
                    .defaultValue = property_value_wrapper{-32.f},
                });

                myU32 = options.register_option({
                    .kind = property_kind::u32,
                    .id = "25b6a724-454c-4405-ac95-fa5350f83e3f"_uuid,
                    .name = "myU32",
                    .category = "myCategory",
                    .defaultValue = property_value_wrapper{42u},
                });
            }

            static constexpr uuid layerUuid = "01234567-89ab-cdef-0123-456789abcdef"_uuid;

            options_manager options;
            h32<options_layer> myLayer{};
            h32<option> myBool{};
            h32<option> myF32{};
            h32<option> myU32{};
        };
    }

    TEST_F(options_test, basic)
    {
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

    class options_serialize_test : public options_test
    {
    protected:
        void SetUp() override
        {
            options_test::SetUp();

            ASSERT_TRUE(options.set_option_value(myLayer, myF32, property_value_wrapper{42.f}));
            ASSERT_TRUE(options.set_option_value(myLayer, myBool, property_value_wrapper{true}));

            doc.init();
            options.store_layer(doc, doc.get_root(), myLayer);

            // Clear the layer to the previous values
            ASSERT_TRUE(options.clear_option_value(myLayer, myF32));
            ASSERT_TRUE(options.clear_option_value(myLayer, myBool));

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

        data_document doc;
    };

    TEST_F(options_serialize_test, in_memory)
    {
        options.load_layer(doc, doc.get_root(), myLayer);

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
    }

    TEST_F(options_serialize_test, json)
    {
        constexpr cstring_view testDir{"./test/options_test/"};
        constexpr cstring_view path{"./test/options_test/options_serialize_test.json"};

        ASSERT_TRUE(filesystem::create_directories(testDir));

        ASSERT_TRUE(json::write(doc, path));

        data_document newDoc;
        ASSERT_TRUE(json::read(newDoc, path));

        options.load_layer(newDoc, newDoc.get_root(), myLayer);

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
    }
}