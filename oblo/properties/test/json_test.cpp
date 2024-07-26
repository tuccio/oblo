#include <gtest/gtest.h>

#include <oblo/core/overload.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/properties/serialization/visit.hpp>

namespace oblo
{
    TEST(properties_serialization, json)
    {

        struct values
        {
            bool myBool;
            u32 myU32;
            f32 myF32;
            u32 myArray[2];
        };

        constexpr values expected{
            .myBool = true,
            .myU32 = 42,
            .myF32 = 16.f,
            .myArray = {42, 666},
        };

        {
            data_document doc;

            doc.init();

            const auto root = doc.get_root();

            doc.child_value(root, "myU32", property_kind::u32, std::as_bytes(std::span{&expected.myU32, 1}));
            doc.child_value(root, "myF32", property_kind::f32, std::as_bytes(std::span{&expected.myF32, 1}));
            doc.child_value(root, "myBool", property_kind::boolean, std::as_bytes(std::span{&expected.myBool, 1}));

            const auto array = doc.child_array(root, "myArray", 2);

            u32 i = 0;

            for (auto arrayElement = doc.child_next(array, data_node::Invalid); arrayElement != data_node::Invalid;
                 arrayElement = doc.child_next(array, arrayElement))
            {
                doc.make_value(arrayElement, property_kind::u32, std::as_bytes(std::span{&expected.myArray[i], 1}));
                ++i;
            }

            ASSERT_EQ(i, 2);

            ASSERT_TRUE(json::write(doc, "test/test.json"));
        }

        {
            data_document doc;
            ASSERT_TRUE(json::read(doc, "test/test.json"));

            u32 currentDepth{0};
            u32 maxDepth{0};
            u32 numValues{0};

            i32 arrayIndex{-1};

            values readback{};

            visit(doc,
                overload{[&](const string_view, data_node_object_start)
                    {
                        // We only have the root object
                        ++currentDepth;
                        maxDepth = max(currentDepth, maxDepth);
                        return visit_result::recurse;
                    },
                    [&](const string_view, data_node_object_finish)
                    {
                        // We only have the root object
                        --currentDepth;
                        return visit_result::recurse;
                    },
                    [&](const string_view key, data_node_array_start)
                    {
                        if (key == "myArray" && arrayIndex < 0)
                        {
                            arrayIndex = 0;
                        }

                        return visit_result::recurse;
                    },
                    [&](const string_view key, data_node_array_finish)
                    {
                        if (key == "myArray" && arrayIndex == 2)
                        {
                            arrayIndex = -1;
                        }

                        return visit_result::recurse;
                    },
                    [&](const string_view key, const void* value, property_kind kind, data_node_value)
                    {
                        if (arrayIndex >= 0 && kind == property_kind::u32)
                        {
                            readback.myArray[arrayIndex] = *reinterpret_cast<const u32*>(value);
                            ++arrayIndex;
                            return visit_result::recurse;
                        }

                        switch (kind)
                        {
                        case property_kind::boolean: {
                            if (key == "myBool")
                            {
                                ++numValues;
                                const bool v = *reinterpret_cast<const bool*>(value);
                                readback.myBool = v;
                            }
                        }
                        break;

                        case property_kind::f64: {
                            if (key == "myF32")
                            {
                                ++numValues;
                                const f64 v = *reinterpret_cast<const f64*>(value);
                                readback.myF32 = f32(v);
                            }
                        }
                        break;

                        case property_kind::u32: {
                            if (key == "myU32")
                            {
                                ++numValues;
                                const u32 v = *reinterpret_cast<const u32*>(value);
                                readback.myU32 = v;
                            }
                        }
                        break;

                        default:
                            break;
                        }

                        return visit_result::recurse;
                    }});

            ASSERT_EQ(numValues, 3);

            ASSERT_EQ(expected.myBool, readback.myBool);
            ASSERT_NEAR(expected.myF32, readback.myF32, .0001f);
            ASSERT_EQ(expected.myU32, readback.myU32);
            ASSERT_EQ(expected.myArray[0], readback.myArray[0]);
            ASSERT_EQ(expected.myArray[1], readback.myArray[1]);

            ASSERT_EQ(arrayIndex, -1);
        }
    }
}