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

#if defined(__clang__)

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

#elif defined(_MSC_VER)

        ASSERT_EQ(get_qualified_type_name<test_class>(), "class oblo::test_class");
        ASSERT_EQ(get_qualified_type_name<test_struct>(), "struct oblo::test_struct");

        ASSERT_EQ(get_qualified_type_name<test_enum_class>(), "enum oblo::test_enum_class");
        ASSERT_EQ(get_qualified_type_name<test_enum_struct>(), "enum oblo::test_enum_struct");

        constexpr auto str = get_qualified_type_name<test_template<test_class>>();

        ASSERT_EQ(get_qualified_type_name<test_template<test_class>>(),
            "struct oblo::test_template<class oblo::test_class> ");
        ASSERT_EQ(get_qualified_type_name<test_template<test_struct>>(),
            "struct oblo::test_template<struct oblo::test_struct> ");

        ASSERT_EQ(get_qualified_type_name<test_template<test_enum_class>>(),
            "struct oblo::test_template<enum oblo::test_enum_class> ");
        ASSERT_EQ(get_qualified_type_name<test_template<test_enum_struct>>(),
            "struct oblo::test_template<enum oblo::test_enum_struct> ");

#endif
    }
}