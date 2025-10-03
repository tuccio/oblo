#include <oblo/script/compiler/bytecode_generator.hpp>

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/core/unreachable.hpp>

namespace oblo
{
    namespace
    {
        constexpr bytecode_payload lo16(u32 val)
        {
            return {u16(val & 0xff)};
        }

        constexpr bytecode_payload hi16(u32 val)
        {
            return {u16(val >> 16)};
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
            default:
                unreachable();
                break;

            case ast_node_kind::function_declaration: {
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
                        OBLO_ASSERT(false);
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
            u32 expressionResultSize{}; // Maybe it should be a type instead
        };

        deque<visit_info> visitStack;
        deque<node_info> nodeInfo;

        string_interner readOnlyStringsInterner;
        readOnlyStringsInterner.init(256);

        const auto pushReadOnlyString16 = [&m, &readOnlyStringsInterner](string_view str) -> expected<u16>
        {
            // We use the string interner to make strings unique
            // Id 0 is not used in that to avoid the null handle, so we translate the id
            const h32 id = readOnlyStringsInterner.get_or_add(str);

            if (!id)
            {
                return unspecified_error;
            }

            const u32 translatedId = id.value - 1;

            if (translatedId == m.readOnlyStrings.size())
            {
                m.readOnlyStrings.emplace_back(str);
            }

            if (translatedId > std::numeric_limits<u16>::max())
            {
                return unspecified_error;
            }

            return u16(translatedId);
        };

        for (const ast_function_ref& f : functions)
        {
            u8 returnSize = 0;

            // TODO: Make a more extensible type system
            const ast_node& fDecl = ast.get(f.declaration);

            if (fDecl.node.functionDecl.returnType.empty())
            {
                returnSize = 0;
            }
            else if (fDecl.node.functionDecl.returnType == "f32")
            {
                returnSize = sizeof(f32);
            }
            else
            {
                return unspecified_error;
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

                        m.text.push_back({.op = bytecode_op::push_32lo16, .payload = lo16(val)});
                        m.text.push_back({.op = bytecode_op::or_32hi16, .payload = hi16(val)});
                        thisNodeInfo.expressionResultSize = sizeof(f32);
                    }
                    break;

                    case ast_node_kind::string_constant: {
                        const expected<u16> stringId = pushReadOnlyString16(n.node.string.value);

                        if (!stringId)
                        {
                            return unspecified_error;
                        }

                        m.text.push_back({.op = bytecode_op::push_read_only_string_view, .payload = *stringId});
                        thisNodeInfo.expressionResultSize = script_string_ref_size();
                    }
                    break;

                    case ast_node_kind::binary_operator: {
                        switch (n.node.binaryOp.op)
                        {
                        case ast_binary_operator_kind::add_f32:
                            m.text.push_back({.op = bytecode_op::add_f32});
                            thisNodeInfo.expressionResultSize = sizeof(f32);
                            break;

                        case ast_binary_operator_kind::sub_f32:
                            m.text.push_back({.op = bytecode_op::sub_f32});
                            thisNodeInfo.expressionResultSize = sizeof(f32);
                            break;

                        default:
                            return unspecified_error;
                        }
                    }
                    break;

                    case ast_node_kind::return_statement: {
                        m.text.push_back({.op = bytecode_op::push_value_sizeoffset,
                            .payload = bytecode_payload::pack_2xu8(returnSize, 0)});
                    }
                    break;

                    case ast_node_kind::function_call: {
                        const expected<u16> stringId = pushReadOnlyString16(n.node.functionCall.name);

                        if (!stringId)
                        {
                            return unspecified_error;
                        }

                        m.text.push_back({.op = bytecode_op::call_api_static, .payload = *stringId});

                        // TODO: We might have to consider return types, which might mean we need function declarations
                        // for API calls.
                        thisNodeInfo.expressionResultSize = 0;
                    }
                    break;

                    case ast_node_kind::function_argument: {
                        // TODO: How to deal with function argument? Since we are visiting bottom up we get the argument
                        // before the function call On the stack we should have the argument right now
                    }
                    break;

                    case ast_node_kind::variable_declaration:
                        break;

                    case ast_node_kind::variable_definition: {
                        if (!n.parent)
                        {
                            return unspecified_error;
                        }

                        auto& decl = ast.get(n.parent);

                        if (decl.kind != ast_node_kind::variable_declaration)
                        {
                            return unspecified_error;
                        }

                        const expected varName = pushReadOnlyString16(decl.node.varDecl.name);

                        if (!varName)
                        {
                            return unspecified_error;
                        }

                        const auto children = ast.children(node);

                        if (auto it = children.begin(); it != children.end())
                        {
                            // We can get the expression result type/size from the child
                            const h32<ast_node> childNode = *it;

                            const auto variableSize = nodeInfo[childNode.value].expressionResultSize;
                            thisNodeInfo.expressionResultSize = variableSize;
                            ++it;

                            m.text.push_back({.op = bytecode_op::push_stack_top_ref, .payload = u16(variableSize)});
                            m.text.push_back({.op = bytecode_op::tag_data_ref_static, .payload = *varName});

                            // We expect a single child for the definition, i.e. the expression the variable is
                            // initialized with
                            if (it != children.end())
                            {
                                return unspecified_error;
                            }
                        }
                    }
                    break;

                    case ast_node_kind::variable_reference: {
                        const expected varName = pushReadOnlyString16(n.node.varRef.name);

                        if (!varName)
                        {
                            return unspecified_error;
                        }

                        m.text.push_back({.op = bytecode_op::push_tagged_data_ref_static, .payload = *varName});
                        thisNodeInfo.expressionResultSize = script_data_ref_size();
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

            const auto& functionDecl = ast.get(f.declaration).node.functionDecl;

            auto& newFunction = m.functions.emplace_back();
            newFunction.id = functionDecl.name;
            newFunction.textOffset = textOffset;
            newFunction.returnSize = returnSize;
        }

        return std::move(m);
    }
}