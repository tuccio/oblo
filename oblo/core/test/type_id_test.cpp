#include <gtest/gtest.h>

#include <oblo/core/type_id.hpp>

namespace oblo
{
    class test_class
    {
    };

    struct test_struct
    {
    };

    enum class test_enum_class
    {
    };

    enum struct test_enum_struct
    {
    };

    template <typename T>
    struct test_template
    {
    };

    TEST(type_id, name)
    {
        ASSERT_EQ(get_qualified_type_name<i32>(), "int");
        ASSERT_EQ(get_qualified_type_name<f32>(), "float");

        ASSERT_EQ(get_qualified_type_name<test_class>(), "oblo::test_class");
        ASSERT_EQ(get_qualified_type_name<test_struct>(), "oblo::test_struct");

        ASSERT_EQ(get_qualified_type_name<test_enum_class>(), "oblo::test_enum_class");
        ASSERT_EQ(get_qualified_type_name<test_enum_struct>(), "oblo::test_enum_struct");

        ASSERT_EQ(get_qualified_type_name<test_template<test_class>>(), "oblo::test_template<oblo::test_class>");
        ASSERT_EQ(get_qualified_type_name<test_template<test_struct>>(), "oblo::test_template<oblo::test_struct>");

        ASSERT_EQ(get_qualified_type_name<test_template<test_enum_class>>(),
                  "oblo::test_template<oblo::test_enum_class>");
        ASSERT_EQ(get_qualified_type_name<test_template<test_enum_struct>>(),
                  "oblo::test_template<oblo::test_enum_struct>");
    }
}