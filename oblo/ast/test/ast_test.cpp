#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/dynamic_array.hpp>

#include <gtest/gtest.h>

namespace oblo
{
    TEST(abstract_syntax_tree, add_function_node)
    {
        abstract_syntax_tree ast;
        ast.init();

        ast_function_declaration func{.name = "main", .returnType = "void"};

        const auto root = ast.get_root();

        ast.add_node(root, func);

        const ast_node& node = ast.children(root).begin() != ast.children(root).end()
            ? ast.get(*ast.children(root).begin())
            : throw std::runtime_error("Function node not added");

        ASSERT_EQ(node.kind, ast_node_kind::function_declaration);
        ASSERT_EQ(node.node.functionDecl.name, "main");
        ASSERT_EQ(node.node.functionDecl.returnType, "void");
    }

    TEST(abstract_syntax_tree, add_constants)
    {
        abstract_syntax_tree ast;
        ast.init();

        const auto root = ast.get_root();

        ast.add_node(root, ast_i64_constant{42});
        ast.add_node(root, ast_f32_constant{3.14f});

        auto it = ast.children(root).begin();

        const ast_node& i64Node = ast.get(*it++);
        ASSERT_EQ(i64Node.kind, ast_node_kind::i64_constant);
        ASSERT_EQ(i64Node.node.i64.value, 42);

        const ast_node& f32Node = ast.get(*it++);
        ASSERT_EQ(f32Node.kind, ast_node_kind::f32_constant);
        ASSERT_FLOAT_EQ(f32Node.node.f32.value, 3.14f);
    }

    TEST(abstract_syntax_tree, child_linking)
    {
        abstract_syntax_tree ast;
        ast.init();

        const auto root = ast.get_root();

        ast.add_node(root, ast_function_parameter{"x", "int"});
        ast.add_node(root, ast_function_parameter{"y", "float"});

        dynamic_array<hashed_string_view> names;

        for (auto childId : ast.children(root))
        {
            const auto& child = ast.get(childId);
            ASSERT_EQ(child.kind, ast_node_kind::function_parameter);
            names.push_back(child.node.functionParameter.name);
        }

        ASSERT_EQ(names.size(), 2);
        ASSERT_EQ(names[0], "x");
        ASSERT_EQ(names[1], "y");
    }
}
