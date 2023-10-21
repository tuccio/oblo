#include <gtest/gtest.h>

#include <oblo/reflection/access.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/registration/registrant.hpp>

namespace oblo::reflection
{
    namespace
    {
        struct test_pod
        {
            i8 i8Field;
            u8 u8Field;
            i16 i16Field;
            u16 u16Field;
            i32 i32Field;
            u32 u32Field;
            i64 i64Field;
            u64 u64Field;
            f32 f32Field;
            f64 f64Field;
            char charField;
            char8_t char8Field;
            wchar_t wharField;
        };

        auto find_field_by_name(std::span<const field_data> fields, const std::string_view name)
        {
            return find_if(fields.begin(),
                fields.end(),
                [name](const field_data& field) { return field.name == name; });
        }
    }

    TEST(fields_reflection, pod_fields)
    {
        reflection_registry reg;

        auto registrant = reg.get_registrant();

        registrant.add_class<test_pod>()
            .add_field(&test_pod::i8Field, "i8Field")
            .add_field(&test_pod::u8Field, "u8Field")
            .add_field(&test_pod::i16Field, "i16Field")
            .add_field(&test_pod::u16Field, "u16Field")
            .add_field(&test_pod::i32Field, "i32Field")
            .add_field(&test_pod::u32Field, "u32Field")
            .add_field(&test_pod::i64Field, "i64Field")
            .add_field(&test_pod::u64Field, "u64Field")
            .add_field(&test_pod::f32Field, "f32Field")
            .add_field(&test_pod::f64Field, "f64Field")
            .add_field(&test_pod::charField, "charField")
            .add_field(&test_pod::char8Field, "char8Field")
            .add_field(&test_pod::wharField, "wharField");

        const class_handle podType = reg.find_class<test_pod>();
        ASSERT_TRUE(podType);

        const std::span fields = reg.get_fields(podType);

        ASSERT_EQ(fields.size(), 13);

        test_pod theStruct{};

        {
            const auto it = find_field_by_name(fields, "i8Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, i8Field));
            ASSERT_EQ(it->type, get_type_id<i8>());

            auto& fRef = access_field<i8>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.i8Field, 0);
            fRef = 42;
            ASSERT_EQ(theStruct.i8Field, 42);
        }

        {
            const auto it = find_field_by_name(fields, "u8Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, u8Field));
            ASSERT_EQ(it->type, get_type_id<u8>());

            auto& fRef = access_field<u8>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.u8Field, 0);
            fRef = 42;
            ASSERT_EQ(theStruct.u8Field, 42);
        }

        {
            const auto it = find_field_by_name(fields, "i16Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, i16Field));
            ASSERT_EQ(it->type, get_type_id<i16>());

            auto& fRef = access_field<i16>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.i16Field, 0);
            fRef = 42;
            ASSERT_EQ(theStruct.i16Field, 42);
        }

        {
            const auto it = find_field_by_name(fields, "u16Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, u16Field));
            ASSERT_EQ(it->type, get_type_id<u16>());

            auto& fRef = access_field<u16>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.u16Field, 0);
            fRef = 42;
            ASSERT_EQ(theStruct.u16Field, 42);
        }

        {
            const auto it = find_field_by_name(fields, "i32Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, i32Field));
            ASSERT_EQ(it->type, get_type_id<i32>());

            auto& fRef = access_field<i32>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.i32Field, 0);
            fRef = 42;
            ASSERT_EQ(theStruct.i32Field, 42);
        }

        {
            const auto it = find_field_by_name(fields, "u32Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, u32Field));
            ASSERT_EQ(it->type, get_type_id<u32>());

            auto& fRef = access_field<u32>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.u32Field, 0);
            fRef = 42;
            ASSERT_EQ(theStruct.u32Field, 42);
        }

        {
            const auto it = find_field_by_name(fields, "i64Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, i64Field));
            ASSERT_EQ(it->type, get_type_id<i64>());

            auto& fRef = access_field<i64>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.i64Field, 0);
            fRef = 42;
            ASSERT_EQ(theStruct.i64Field, 42);
        }

        {
            const auto it = find_field_by_name(fields, "u64Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, u64Field));
            ASSERT_EQ(it->type, get_type_id<u64>());

            auto& fRef = access_field<u64>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.u64Field, 0);
            fRef = 42;
            ASSERT_EQ(theStruct.u64Field, 42);
        }

        {
            const auto it = find_field_by_name(fields, "f32Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, f32Field));
            ASSERT_EQ(it->type, get_type_id<f32>());

            auto& fRef = access_field<f32>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.f32Field, 0.f);
            fRef = 42.f;
            ASSERT_EQ(theStruct.f32Field, 42.f);
        }

        {
            const auto it = find_field_by_name(fields, "f64Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, f64Field));
            ASSERT_EQ(it->type, get_type_id<f64>());

            auto& fRef = access_field<f64>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.f64Field, 0.);
            fRef = 42.;
            ASSERT_EQ(theStruct.f64Field, 42.);
        }

        {
            const auto it = find_field_by_name(fields, "charField");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, charField));
            ASSERT_EQ(it->type, get_type_id<char>());

            auto& fRef = access_field<char>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.charField, 0);
            fRef = 42;
            ASSERT_EQ(theStruct.charField, 42);
        }

        {
            const auto it = find_field_by_name(fields, "char8Field");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, char8Field));
            ASSERT_EQ(it->type, get_type_id<char8_t>());

            auto& fRef = access_field<char8_t>(&theStruct, it->offset);
            ASSERT_EQ(u8{fRef}, 0);
            ASSERT_EQ(u8{theStruct.char8Field}, 0);
            fRef = 42;
            ASSERT_EQ(u8{theStruct.char8Field}, 42);
        }

        {
            const auto it = find_field_by_name(fields, "wharField");
            ASSERT_NE(it, fields.end());
            ASSERT_EQ(it->offset, offsetof(test_pod, wharField));
            ASSERT_EQ(it->type, get_type_id<wchar_t>());

            auto& fRef = access_field<wchar_t>(&theStruct, it->offset);
            ASSERT_EQ(fRef, 0);
            ASSERT_EQ(theStruct.wharField, 0);
            fRef = 42;
            ASSERT_EQ(theStruct.wharField, 42);
        }
    }
}