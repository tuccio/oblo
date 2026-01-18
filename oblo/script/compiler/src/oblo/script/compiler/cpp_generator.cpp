#include <oblo/script/compiler/cpp_generator.hpp>

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/core/string/transparent_string_hash.hpp>
#include <oblo/core/unreachable.hpp>

#include <unordered_map>
#include <unordered_set>

#include <cppgenlib.gen.hpp>

namespace oblo
{
    namespace
    {
        class codegen_helper
        {
        public:
            explicit codegen_helper(string_builder& b) : m_builder{b} {}

            void indent()
            {
                ++m_indentationDepth;
            }

            void deindent()
            {
                OBLO_ASSERT(m_indentationDepth > 0);
                --m_indentationDepth;
            }

            void begin_scope()
            {
                append('{');
                new_line();
                indent();
            }

            void end_scope()
            {
                deindent();
                append('}');
                new_line();
            }

            [[nodiscard]] auto begin_scope_raii()
            {
                begin_scope();
                return finally([this] { end_scope(); });
            }

            void new_line()
            {
                m_builder.append('\n');
                m_pendingIndentation = true;
            }

            void append(char c)
            {
                apply_indentation();
                m_builder.append(c);
            }

            void append(string_view str)
            {
                apply_indentation();
                m_builder.append(str);
            }

            template <typename... T>
            void format(std::format_string<T...> fmt, T&&... args)
            {
                apply_indentation();
                m_builder.format(fmt, std::forward<T>(args)...);
            }

            template <typename Iterator>
            void join(Iterator&& begin,
                Iterator&& end,
                string_view separator,
                std::format_string<decltype(*std::declval<Iterator>())> fmt)
            {
                apply_indentation();
                m_builder.join(std::forward<Iterator>(begin), std::forward<Iterator>(end), separator, fmt);
            }

        private:
            void apply_indentation()
            {
                if (m_pendingIndentation)
                {
                    m_pendingIndentation = false;

                    for (u32 i = 0; i < m_indentationDepth; ++i)
                    {
                        m_builder.append('\t');
                    }
                }
            }

        private:
            string_builder& m_builder;
            u32 m_indentationDepth{0};
            bool m_pendingIndentation{true};
        };

        void append_var_name(auto&& g, h32<ast_node> n)
        {
            g.format("_n{}", n.value);
        }

        class statement_helper : string_builder
        {
        public:
            using string_builder::append;
            using string_builder::format;

            void begin()
            {
                string_builder::clear();
                m_isCompileTime = false;
                m_isExpression = true;
                m_expressionType = "auto";
            }

            void finalize(codegen_helper& g, h32<ast_node> n)
            {
                if (!m_isExpression)
                {
                    g.append(string_builder::as<string_view>());
                    g.append(';');
                    g.new_line();
                    return;
                }

                g.append(m_isCompileTime ? "constexpr " : "const ");
                g.append(m_expressionType.as<string_view>());
                g.append(' ');
                append_var_name(g, n);
                g.append(" = ");
                g.append(string_builder::as<string_view>());
                g.append(';');
                g.new_line();
            }

            void set_is_compile_time(bool isCompileTime)
            {
                m_isCompileTime = isCompileTime;
            }

            void set_is_expression(bool isExpression)
            {
                m_isExpression = isExpression;
            }

            void set_expression_type(string_view type)
            {
                m_expressionType = type;
            }

        private:
            bool m_isCompileTime;
            bool m_isExpression;
            string_builder m_expressionType;
        };

        bool get_children(const abstract_syntax_tree& ast, h32<ast_node> node, const std::span<h32<ast_node>> out)
        {
            const auto children = ast.children(node);

            auto it = children.begin();

            for (u32 i = 0; i < out.size(); ++i)
            {
                if (it == children.end())
                {
                    return false;
                }


                out[i] = *it;
                ++it;
            }

            return true;
        }

        void append_all_arguments(const abstract_syntax_tree& ast, h32<ast_node> node, statement_helper& stmt)
        {
            bool isFirst = true;

            for (const h32 child : ast.children(node))
            {
                auto& childNode = ast.get(child);

                if (childNode.kind != ast_node_kind::function_argument)
                {
                    continue;
                }

                if (!isFirst)
                {
                    stmt.append(", ");
                }

                isFirst = false;
                append_var_name(stmt, child);
            }
        }
    }

    expected<string_builder> cpp_generator::generate_code(const abstract_syntax_tree& ast)
    {
        string_builder code;
        codegen_helper g{code};

        // Our preamble is parsed from a header we embed using CMake
        g.append(gen::cppgenlib);

        struct ast_function_ref
        {
            h32<ast_node> declaration;
            h32<ast_node> body;
        };

        struct ast_function_decl_ref
        {
            hashed_string_view returnType;
        };

        struct ast_type_ref
        {
            u8 size;
        };

        deque<ast_function_ref> functions;

        std::unordered_set<hashed_string_view, hash<hashed_string_view>> referencedFunctions;
        std::unordered_map<hashed_string_view, ast_function_decl_ref, transparent_string_hash> functionDeclarations;
        std::unordered_map<hashed_string_view, ast_type_ref, transparent_string_hash> types;

        dynamic_array<h32<ast_node>> forwardDeclaredFunctions;
        forwardDeclaredFunctions.reserve(64);

        for (const h32 childHandle : ast.children(ast.get_root()))
        {
            const ast_node& node = ast.get(childHandle);

            switch (node.kind)
            {
            default:
                unreachable();
                break;

            case ast_node_kind::type_declaration:
                types[node.node.typeDecl.name] = {
                    .size = node.node.typeDecl.size,
                };

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

                    case ast_node_kind::function_parameter:
                        // Nothing to do here
                        break;

                    default:
                        OBLO_ASSERT(false);
                        return unspecified_error;
                    }
                }

                if (body)
                {
                    functions.push_back({
                        .declaration = childHandle,
                        .body = body,
                    });
                }
                else
                {
                    forwardDeclaredFunctions.emplace_back(childHandle);
                }

                functionDeclarations[node.node.functionDecl.name] = {
                    .returnType = node.node.functionDecl.returnType,
                };
            }

            break;
            }
        }

        {

            // Generate forward declarations as dynamically loaded function pointers
            // The oblo_load_symbols function generated at the end will import all (and only) the referenced functions.
            g.append("namespace");
            g.new_line();

            const auto anonNamespace = g.begin_scope_raii();

            for (const h32 functionDeclaration : forwardDeclaredFunctions)
            {
                const auto& node = ast.get(functionDeclaration);
                OBLO_ASSERT(node.kind == ast_node_kind::function_declaration);

                g.format("using {}_fn_t = {}(*)(", node.node.functionDecl.name, node.node.functionDecl.returnType);

                bool isFirst = true;

                for (const h32 child : ast.children(functionDeclaration))
                {
                    const auto& childNode = ast.get(child);

                    if (childNode.kind != ast_node_kind::function_parameter)
                    {
                        continue;
                    }

                    if (!isFirst)
                    {
                        g.append(", ");
                    }

                    isFirst = false;

                    g.append(childNode.node.functionParameter.type);
                    g.append(' ');
                    g.append(childNode.node.functionParameter.name);
                }

                g.append(");");

                g.new_line();

                g.format("{0}_fn_t {0} [[maybe_unused]] = nullptr;", node.node.functionDecl.name);
                g.new_line();

                functionDeclarations[node.node.functionDecl.name] = {
                    .returnType = node.node.functionDecl.returnType,
                };
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
            const ast_node& fDecl = ast.get(f.declaration);

            if (!types.contains(fDecl.node.functionDecl.returnType))
            {
                return unspecified_error;
            }

            g.append("extern \"C\" OBLO_SHARED_LIBRARY_EXPORT ");
            g.append(fDecl.node.functionDecl.returnType);
            g.append(' ');
            g.append(fDecl.node.functionDecl.name);

            // Function arguments would need to be handled here
            g.append("()");

            if (!f.body)
            {
                g.append(';');
                continue;
            }

            g.new_line();

            const auto functionDefinitionScope = g.begin_scope_raii();

            for (const h32 child : ast.children(f.body))
            {
                visitStack.emplace_back(child);
            }

            statement_helper stmt;

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

                    stmt.begin();

                    switch (n.kind)
                    {
                    case ast_node_kind::i32_constant:
                        stmt.format("{}", n.node.i32.value);
                        stmt.set_is_compile_time(true);
                        stmt.set_expression_type("i32");
                        // thisNodeInfo.expressionType = "i32";
                        break;

                    case ast_node_kind::u32_constant:
                        stmt.format("{}u", n.node.u32.value);
                        stmt.set_is_compile_time(true);
                        stmt.set_expression_type("u32");
                        // thisNodeInfo.expressionType = "u32";
                        break;

                    case ast_node_kind::f32_constant:
                        stmt.format("{}", n.node.f32.value);
                        stmt.set_is_compile_time(true);
                        stmt.set_expression_type("f32");
                        // thisNodeInfo.expressionType = "f32";
                        break;

                    case ast_node_kind::string_constant:
                        stmt.append('"');
                        stmt.append(n.node.string.value);
                        stmt.append('"');
                        stmt.set_is_compile_time(true);
                        stmt.set_expression_type("const char*");
                        break;

                    case ast_node_kind::binary_operator: {
                        const auto write_binary_operation = [&stmt, &ast, node](string_view op, string_view type)
                        {
                            h32<ast_node> operands[2];

                            if (!get_children(ast, node, operands))
                            {
                                return false;
                            }

                            append_var_name(stmt, operands[0]);
                            stmt.append(op);
                            append_var_name(stmt, operands[1]);

                            stmt.set_expression_type(type);

                            return true;
                        };

                        switch (n.node.binaryOp.op)
                        {
                        case ast_binary_operator_kind::add_f32:
                            if (!write_binary_operation("+", "f32"))
                            {
                                return unspecified_error;
                            }
                            // thisNodeInfo.expressionResultSize = sizeof(u32);
                            break;

                        case ast_binary_operator_kind::sub_f32:
                            if (!write_binary_operation("-", "f32"))
                            {
                                return unspecified_error;
                            }
                            // thisNodeInfo.expressionResultSize = sizeof(f32);
                            break;

                        case ast_binary_operator_kind::mul_f32:
                            if (!write_binary_operation("*", "f32"))
                            {
                                return unspecified_error;
                            }
                            // thisNodeInfo.expressionResultSize = sizeof(f32);
                            break;

                        case ast_binary_operator_kind::div_f32:
                            if (!write_binary_operation("/", "f32"))
                            {
                                return unspecified_error;
                            }
                            // thisNodeInfo.expressionResultSize = sizeof(f32);
                            break;

                        case ast_binary_operator_kind::mul_vec3:
                            if (!write_binary_operation("*", "vec3"))
                            {
                                return unspecified_error;
                            }
                            // thisNodeInfo.expressionResultSize = 3 * sizeof(f32);
                            break;

                        default:
                            return unspecified_error;
                        }
                    }
                    break;

                    case ast_node_kind::return_statement:
                        stmt.append("return ");

                        h32<ast_node> res[1];
                        if (get_children(ast, node, res))
                        {
                            append_var_name(stmt, res[0]);
                        }

                        stmt.set_is_expression(false);
                        break;

                    case ast_node_kind::function_call: {
                        const auto fnIt = functionDeclarations.find(n.node.functionCall.name);

                        if (fnIt == functionDeclarations.end())
                        {
                            return unspecified_error;
                        }

                        referencedFunctions.emplace(n.node.functionCall.name);

                        stmt.append(n.node.functionCall.name);
                        stmt.append('(');

                        append_all_arguments(ast, node, stmt);

                        stmt.append(')');

                        const auto returnTypeIt = types.find(fnIt->second.returnType);

                        if (returnTypeIt == types.end())
                        {
                            return unspecified_error;
                        }

                        if (returnTypeIt->second.size == 0)
                        {
                            stmt.set_is_expression(false);
                        }
                        else
                        {
                            stmt.set_expression_type(fnIt->second.returnType);
                        }
                    }
                    break;

                    case ast_node_kind::function_argument: {
                        // For arguments we just create a variable with a reference, so that the function call can
                        // reference the node result, which is effectively forwarded.
                        h32<ast_node> argExpr[1];

                        if (!get_children(ast, node, argExpr))
                        {
                            return unspecified_error;
                        }

                        append_var_name(stmt, argExpr[0]);
                        stmt.set_expression_type("auto&");
                    }
                    break;

                    case ast_node_kind::variable_declaration:
                        stmt.set_is_expression(false);
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

                        stmt.set_is_expression(false);

                        stmt.format("auto&& {} = ", decl.node.varDecl.name);

                        h32<ast_node> argExpr[1];

                        if (!get_children(ast, node, argExpr))
                        {
                            return unspecified_error;
                        }

                        append_var_name(stmt, argExpr[0]);
                    }
                    break;

                    case ast_node_kind::variable_reference: {
                        stmt.append(n.node.varRef.name);
                    }
                    break;

                    case ast_node_kind::compound:
                        stmt.set_is_expression(false);
                        break;

                    case ast_node_kind::construct_type: {
                        stmt.format("{} {{", n.node.constructType.type);
                        append_all_arguments(ast, node, stmt);
                        stmt.append('}');
                    }

                    break;

                    default:
                        return unspecified_error;
                    }

                    thisNodeInfo.processed = true;
                    visitStack.pop_back();

                    stmt.finalize(g, node);
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
        }

        {
            g.new_line();
            g.append("#define OBLO_LOAD_FUNCTION(Loader, Function) (Function = "
                     "reinterpret_cast<decltype(Function)>(Loader(#Function)))");

            g.new_line();
            g.new_line();
            g.append("extern \"C\" OBLO_SHARED_LIBRARY_EXPORT i32 "
                     "oblo_load_symbols(symbol_loader_fn loader)");

            const auto scope = g.begin_scope_raii();

            g.append("if (!loader) return 0;");
            g.new_line();

            g.append("bool success = true;");
            g.new_line();

            for (const auto& function : referencedFunctions)
            {
                g.format("OBLO_LOAD_FUNCTION(loader, {});", function);
                g.new_line();
            }

            g.append("return i32{success};");
            g.new_line();
        }

        return std::move(code);
    }
}