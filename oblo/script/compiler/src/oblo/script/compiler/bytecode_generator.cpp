#include <oblo/script/compiler/bytecode_generator.hpp>

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/deque.hpp>

namespace oblo
{
    namespace
    {
        constexpr u16 lo16(u32 val)
        {
            return u16(val & 0xff);
        }

        constexpr u16 hi16(u32 val)
        {
            return u16(val >> 16);
        }

    }

    expected<bytecode_module> bytecode_generator::generate_module(const abstract_syntax_tree& ast)
    {
        bytecode_module m;

        struct ast_function_ref
        {
            h32<ast_node> declaration;
            h32<ast_node> body;
        };

        deque<ast_function_ref> functions;

        for (const h32 childHandle : ast.children(ast.get_root()))
        {
            const ast_node& node = ast.get(childHandle);

            switch (node.kind)
            {
            case ast_node_kind::function: {
                // Assuming there are no parameters here
                h32<ast_node> body{};

                for (const h32 functionChildHandle : ast.children(childHandle))
                {
                    const ast_node& functionChild = ast.get(functionChildHandle);

                    switch (functionChild.kind)
                    {
                    case ast_node_kind::function_body:
                        body = functionChildHandle;
                        break;

                    default:
                        return unspecified_error;
                    }
                }

                if (body)
                {
                    functions.push_back({.declaration = childHandle, .body = body});
                }
            }

            break;
            }
        }

        struct visit_info
        {
            h32<ast_node> node{};
        };

        struct node_info
        {
            bool visited{};
            bool processed{};
        };

        deque<visit_info> visitStack;
        deque<node_info> nodeInfo;

        for (const ast_function_ref& f : functions)
        {
            u8 returnSize = 0;

            // TODO: Make a more extensible type system
            if (const ast_node& decl = ast.get(f.declaration); decl.node.function.returnType == "f32")
            {
                returnSize = sizeof(f32);
            }

            const u32 textOffset = m.text.size32();

            for (const h32 child : ast.children(f.body))
            {
                visitStack.emplace_back(child);
            }

            while (!visitStack.empty())
            {
                const h32 node = visitStack.back().node;

                if (node.value >= nodeInfo.size())
                {
                    nodeInfo.resize(node.value + 1);
                }

                auto& thisNodeInfo = nodeInfo[node.value];

                if (thisNodeInfo.processed)
                {
                    visitStack.pop_back();
                }
                else if (thisNodeInfo.visited)
                {
                    const ast_node& n = ast.get(node);

                    switch (n.kind)
                    {
                    case ast_node_kind::f32_constant: {
                        const u32 val = std::bit_cast<u32>(n.node.f32.value);

                        m.text.push_back({.op = bytecode_op::push32lo16, .payload = lo16(val)});
                        m.text.push_back({.op = bytecode_op::or32hi16, .payload = hi16(val)});
                    }
                    break;

                    case ast_node_kind::binary_operator: {
                        switch (n.node.binaryOp.op)
                        {
                        case ast_binary_operator_kind::add_f32:
                            m.text.push_back({.op = bytecode_op::addf32});
                            break;

                        case ast_binary_operator_kind::sub_f32:
                            m.text.push_back({.op = bytecode_op::subf32});
                            break;

                        default:
                            return unspecified_error;
                        }
                    }
                    break;

                    case ast_node_kind::return_statement: {
                        m.text.push_back(
                            {.op = bytecode_op::retvpso, .payload = bytecode_payload::pack_2xu8(returnSize, 0)});
                    }
                    break;

                    default:
                        return unspecified_error;
                    }

                    thisNodeInfo.processed = true;
                    visitStack.pop_back();
                }
                else
                {
                    thisNodeInfo.visited = true;

                    for (const h32 child : ast.children(node))
                    {
                        visitStack.emplace_back(child);
                    }
                }
            }

            m.text.push_back({.op = bytecode_op::ret});

            const auto& functionDecl = ast.get(f.declaration).node.function;

            auto& newFunction = m.functions.emplace_back();
            newFunction.id = functionDecl.name;
            newFunction.textOffset = textOffset;
            newFunction.returnSize = returnSize;
        }

        return std::move(m);
    }
}