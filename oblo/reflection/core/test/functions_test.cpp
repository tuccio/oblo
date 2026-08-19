#include <gtest/gtest.h>

#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/registration/registrant.hpp>

#include <cmath>

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

        const auto addFn = +[](i32 a, i32 b) -> i32 { return a + b; };

        const function_handle fn = registrant.add_function("oblo::math::add", addFn);
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

        const auto getDeltaFn = +[]() -> f32 { return 0.f; };

        const function_handle fn = registrant.add_function("oblo::time::get_delta", getDeltaFn);
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

        const auto setFn = +[](f32, f32) -> void {};

        const function_handle fn = registrant.add_function("oblo::math::set", setFn);
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

        const auto addFn = +[](i32 a, i32 b) -> i32 { return a + b; };
        const auto subFn = +[](i32 a, i32 b) -> i32 { return a - b; };

        const function_handle fn1 = registrant.add_function("oblo::math::add", addFn);
        const function_handle fn2 = registrant.add_function("oblo::math::sub", subFn);

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

        const auto cosFn = +[](f32 x) -> f32 { return std::cos(x); };

        registrant.add_function("oblo::math::cos", cosFn).add_tag<function_test_tag>();

        deque<type_handle> foundFunctions;
        reg.find_by_tag<function_test_tag>(foundFunctions);

        ASSERT_EQ(foundFunctions.size(), 1);
    }
}