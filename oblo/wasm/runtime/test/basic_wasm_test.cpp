#include <gtest/gtest.h>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/wasm/wasm_module.hpp>
#include <oblo/wasm/wasm_runtime_module.hpp>

namespace oblo
{
    class wasm_test : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(mm.load<wasm_runtime_module>());
            ASSERT_TRUE(mm.finalize());
        }

    private:
        module_manager mm;
    };

    TEST_F(wasm_test, add_i32)
    {
        wasm_module module;

        dynamic_array<byte> wasmFile;

        ASSERT_TRUE(
            filesystem::load_binary_file_into_memory(wasmFile, OBLO_TEST_ASSETS_DIR "/wasm/basic_wasm_test.wasm"));

        ASSERT_TRUE(module.load(wasmFile));

        wasm_module_executor exec;
        ASSERT_TRUE(exec.create(module, 1u << 10, 8u << 10));

        const auto addI32 = exec.find_function("add_i32");

        ASSERT_TRUE(addI32);

        const auto res = exec.invoke<int>(addI32, 23, 19);
        ASSERT_TRUE(res);

        ASSERT_EQ(*res, 42);
    }
}