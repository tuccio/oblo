#include <gtest/gtest.h>

#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/reflection/struct_compare.hpp>
#include <oblo/script/resources/compiled_script.hpp>

namespace oblo
{
    TEST(compiled_script, serialization)
    {
        compiled_script src;

        src.module.functions = {
            exported_function{
                .id = "exported_func",
                .paramsSize = 1,
                .returnSize = 16,
                .textOffset = 4,
            },
        };

        src.module.readOnlyStrings = {"my_string"};

        src.module.text = {
            {bytecode_op::callapipu16, bytecode_payload::pack_u16(0)},
            {bytecode_op::ret},
        };

        ASSERT_TRUE(save(src, "serialization.bytecode"));

        compiled_script dst;
        ASSERT_TRUE(load(dst, "serialization.bytecode"));

        ASSERT_EQ(src.module.functions.size(), dst.module.functions.size());
        ASSERT_EQ(src.module.text.size(), dst.module.text.size());
        ASSERT_EQ(src.module.readOnlyStrings.size(), dst.module.readOnlyStrings.size());

        for (usize i = 0; i < src.module.functions.size(); ++i)
        {
            ASSERT_TRUE(struct_compare<std::equal_to>(src.module.functions[i], dst.module.functions[i]));
        }

        for (usize i = 0; i < src.module.text.size(); ++i)
        {
            ASSERT_TRUE(struct_compare<std::equal_to>(src.module.text[i], dst.module.text[i]));
        }

        for (usize i = 0; i < src.module.readOnlyStrings.size(); ++i)
        {
            ASSERT_TRUE(struct_compare<std::equal_to>(src.module.readOnlyStrings[i], dst.module.readOnlyStrings[i]));
        }
    }
}