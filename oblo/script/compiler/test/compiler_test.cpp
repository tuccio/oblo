#include <oblo/script/compiler/bytecode_generator.hpp>

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/script/interpreter.hpp>

#include <gtest/gtest.h>

namespace oblo
{
    TEST(bytecode_generator, add_sub_f32_constants)
    {
        abstract_syntax_tree ast;
        ast.init();

        const auto root = ast.get_root();

        const auto hFunc = ast.add_node(root, ast_function_declaration{.name = "add_sub", .returnType = "f32"});
        const auto hBody = ast.add_node(hFunc, ast_function_body{});
        const auto hReturn = ast.add_node(hBody, ast_return_statement{});

        const auto hAdd = ast.add_node(hReturn, ast_binary_operator{.op = ast_binary_operator_kind::add_f32});
        ast.add_node(hAdd, ast_f32_constant{40.f});

        const auto hSub = ast.add_node(hAdd, ast_binary_operator{.op = ast_binary_operator_kind::sub_f32});
        ast.add_node(hSub, ast_f32_constant{5.f});
        ast.add_node(hSub, ast_f32_constant{3.f});

        bytecode_generator gen;
        const auto m = gen.generate_module(ast);
        ASSERT_TRUE(m);

        interpreter rt;
        rt.init(1u << 8);

        rt.load_module(*m);
        const h32 hFuncInstance = rt.find_function("add_sub"_hsv);
        ASSERT_TRUE(hFuncInstance);

        ASSERT_TRUE(rt.call_function(hFuncInstance));

        // We should have the result at the top
        const expected<f32, interpreter_error> r = rt.read_f32(0);
        ASSERT_TRUE(r);

        ASSERT_FLOAT_EQ(*r, 42.f);
    }
}