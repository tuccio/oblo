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

        const auto res = exec.invoke<i32>(addI32, 23, 19);
        ASSERT_TRUE(res);

        ASSERT_EQ(*res, 42);
    }

    TEST_F(wasm_test, add_u32)
    {
        wasm_module module;

        dynamic_array<byte> wasmFile;

        ASSERT_TRUE(
            filesystem::load_binary_file_into_memory(wasmFile, OBLO_TEST_ASSETS_DIR "/wasm/basic_wasm_test.wasm"));

        ASSERT_TRUE(module.load(wasmFile));

        wasm_module_executor exec;
        ASSERT_TRUE(exec.create(module, 1u << 10, 8u << 10));

        const auto addU32 = exec.find_function("add_u32");

        ASSERT_TRUE(addU32);

        constexpr u32 a = u32(std::numeric_limits<i32>::max());
        constexpr u32 b = 100u;

        const auto res = exec.invoke<u32>(addU32, a, b);
        ASSERT_TRUE(res);

        ASSERT_EQ(*res, a + b);
    }

    TEST_F(wasm_test, global)
    {
        wasm_module module;

        dynamic_array<byte> wasmFile;

        ASSERT_TRUE(
            filesystem::load_binary_file_into_memory(wasmFile, OBLO_TEST_ASSETS_DIR "/wasm/basic_wasm_test.wasm"));

        ASSERT_TRUE(module.load(wasmFile));

        wasm_module_executor exec;
        ASSERT_TRUE(exec.create(module, 1u << 10, 8u << 10));

        const auto getGlobalValue = exec.find_function("get_global_value");
        const auto setGlobalValue = exec.find_function("set_global_value");

        ASSERT_TRUE(getGlobalValue);
        ASSERT_TRUE(setGlobalValue);

        const auto initialValue = exec.invoke<u32>(getGlobalValue);
        ASSERT_TRUE(initialValue);

        ASSERT_EQ(*initialValue, 0);

        const auto setResult = exec.invoke<void>(setGlobalValue, 42);
        ASSERT_TRUE(setResult);

        const auto finalValue = exec.invoke<u32>(getGlobalValue);
        ASSERT_TRUE(finalValue);

        ASSERT_EQ(*finalValue, 42);
    }
}