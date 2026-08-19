#include <gtest/gtest.h>

#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/registration/registrant.hpp>

namespace oblo::reflection
{
    namespace
    {
        struct function_test_tag
        {
        };
    }

    TEST(functions_reflection, add_function_with_parameters)
    {
        reflection_registry reg;

        auto registrant = reg.get_registrant();

        const function_handle fn = registrant.add_function<i32, i32, i32>("oblo::math::add");
        ASSERT_TRUE(fn);

        const function_data data = reg.get_function_data(fn);
        ASSERT_EQ(data.fullyQualifiedName, "oblo::math::add");
        ASSERT_EQ(data.returnType, get_type_id<i32>());
        ASSERT_EQ(data.parameterTypes.size(), 2);
        ASSERT_EQ(data.parameterTypes[0], get_type_id<i32>());
        ASSERT_EQ(data.parameterTypes[1], get_type_id<i32>());
    }

    TEST(functions_reflection, add_function_without_parameters)
    {
        reflection_registry reg;

        auto registrant = reg.get_registrant();

        const function_handle fn = registrant.add_function<f32>("oblo::time::get_delta");
        ASSERT_TRUE(fn);

        const function_data data = reg.get_function_data(fn);
        ASSERT_EQ(data.fullyQualifiedName, "oblo::time::get_delta");
        ASSERT_EQ(data.returnType, get_type_id<f32>());
        ASSERT_TRUE(data.parameterTypes.empty());
    }

    TEST(functions_reflection, add_void_function)
    {
        reflection_registry reg;

        auto registrant = reg.get_registrant();

        const function_handle fn = registrant.add_function<void, f32, f32>("oblo::math::set");
        ASSERT_TRUE(fn);

        const function_data data = reg.get_function_data(fn);
        ASSERT_EQ(data.fullyQualifiedName, "oblo::math::set");
        ASSERT_EQ(data.returnType, get_type_id<void>());
        ASSERT_EQ(data.parameterTypes.size(), 2);
        ASSERT_EQ(data.parameterTypes[0], get_type_id<f32>());
        ASSERT_EQ(data.parameterTypes[1], get_type_id<f32>());
    }

    TEST(functions_reflection, distinct_functions_get_distinct_handles)
    {
        reflection_registry reg;

        auto registrant = reg.get_registrant();

        const function_handle fn1 = registrant.add_function<i32, i32, i32>("oblo::math::add");
        const function_handle fn2 = registrant.add_function<i32, i32, i32>("oblo::math::sub");

        ASSERT_TRUE(fn1);
        ASSERT_TRUE(fn2);
        ASSERT_NE(fn1, fn2);
        ASSERT_EQ(reg.get_function_data(fn1).fullyQualifiedName, "oblo::math::add");
        ASSERT_EQ(reg.get_function_data(fn2).fullyQualifiedName, "oblo::math::sub");
    }

    TEST(functions_reflection, add_function_tag)
    {
        reflection_registry reg;

        auto registrant = reg.get_registrant();

        registrant.add_function<f32, f32>("oblo::math::cos").add_tag<function_test_tag>();

        deque<type_handle> foundFunctions;
        reg.find_by_tag<function_test_tag>(foundFunctions);

        ASSERT_EQ(foundFunctions.size(), 1);
    }
}