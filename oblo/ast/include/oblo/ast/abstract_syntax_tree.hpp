#pragma once

#include <oblo/core/any.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/iterator/iterator_range.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_interner.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    struct ast_node;

    enum class ast_node_kind : u8
    {
        root,
        type_declaration,
        function_declaration,
        function_parameter,
        function_body,
        function_call,
        function_argument,
        compound,
        binary_operator,
        i32_constant,
        u32_constant,
        i64_constant,
        u64_constant,
        f32_constant,
        f64_constant,
        string_constant,
        variable_declaration,
        variable_definition,
        variable_reference,
        return_statement,
    };

    enum class ast_binary_operator_kind
    {
        add_f32,
        div_f32,
        mul_f32,
        sub_f32,
    };

    struct ast_root
    {
    };

    struct ast_type_declaration
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::type_declaration;

        hashed_string_view name;
        u8 size;
    };

    struct ast_function_declaration
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::function_declaration;

        hashed_string_view name;
        hashed_string_view returnType;
    };

    struct ast_function_parameter
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::function_parameter;

        hashed_string_view name;
        hashed_string_view type;
    };

    struct ast_function_body
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::function_body;
    };

    struct ast_function_call
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::function_call;

        hashed_string_view name;
    };

    struct ast_function_argument
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::function_argument;

        hashed_string_view name;
    };

    struct ast_compound
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::compound;
    };

    struct ast_binary_operator
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::binary_operator;

        ast_binary_operator_kind op;
    };

    struct ast_i32_constant
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::i32_constant;

        i32 value;
    };

    struct ast_u32_constant
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::u32_constant;

        u32 value;
    };

    struct ast_i64_constant
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::i64_constant;

        i64 value;
    };

    struct ast_u64_constant
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::u64_constant;

        u64 value;
    };

    struct ast_f32_constant
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::f32_constant;

        f32 value;
    };

    struct ast_f64_constant
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::f64_constant;

        f64 value;
    };

    struct ast_string_constant
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::string_constant;

        hashed_string_view value;
        h32<string> interned{};
    };

    struct ast_variable_declaration
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::variable_declaration;
        hashed_string_view name;
    };

    struct ast_variable_definition
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::variable_definition;
    };

    struct ast_variable_reference
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::variable_reference;
        hashed_string_view name;
    };

    struct ast_return_statement
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::return_statement;
    };

    struct ast_node
    {
        h32<ast_node> firstChild{};
        h32<ast_node> lastChild{};
        h32<ast_node> prevSibling{};
        h32<ast_node> nextSibling{};
        h32<ast_node> parent{};

        ast_node_kind kind{ast_node_kind::root};

        union {
            ast_root root;
            ast_type_declaration typeDecl;
            ast_function_declaration functionDecl;
            ast_function_parameter functionParameter;
            ast_function_body functionBody;
            ast_function_call functionCall;
            ast_function_argument functionArgument;
            ast_compound compound;
            ast_binary_operator binaryOp;
            ast_i32_constant i32;
            ast_u32_constant u32;
            ast_i64_constant i64;
            ast_u64_constant u64;
            ast_f32_constant f32;
            ast_f64_constant f64;
            ast_string_constant string;
            ast_variable_declaration varDecl;
            ast_variable_definition varDef;
            ast_variable_reference varRef;
            ast_return_statement retStmt;
        } node = {.root = {}};
    };

    /// @brief Abstract syntax tree for stripped down C-like languages.
    class abstract_syntax_tree
    {
    public:
        class children_iterator;

    public:
        void init();

        bool is_initialized() const;

        h32<ast_node> get_root() const;

        h32<ast_node> get_parent(h32<ast_node>) const;

        template <typename T>
        h32<ast_node> add_node(h32<ast_node> parent, T&& node);

        void reparent(h32<ast_node> node, h32<ast_node> newParent);

        /// @brief Removes the links of the node from its parent, without destroying the node or the subtree, but
        /// effectively excluding it from visits.
        /// @param root The root of the subtree to unlink.
        void unlink_subtree(h32<ast_node> root);

        h32<ast_node> child_next(h32<ast_node> parent, h32<ast_node> previous) const;
        h32<ast_node> child_prev(h32<ast_node> parent, h32<ast_node> next) const;
        iterator_range<children_iterator> children(h32<ast_node> node) const;

        const ast_node& get(h32<ast_node> node) const;
        ast_node& get(h32<ast_node> node);

    private:
        void link_parent(h32<ast_node> parent, h32<ast_node> child);
        void unlink_parent(h32<ast_node> child);

        void set_node(ast_node& n, const ast_type_declaration& v)
        {
            n.kind = ast_node_kind::type_declaration;
            n.node.typeDecl = v;
            n.node.typeDecl.name = intern_h_str(v.name);
        }

        void set_node(ast_node& n, const ast_function_declaration& v)
        {
            n.kind = ast_node_kind::function_declaration;
            n.node.functionDecl = v;
            n.node.functionDecl.name = intern_h_str(v.name);
            n.node.functionDecl.returnType = intern_h_str(v.returnType);
        }

        void set_node(ast_node& n, const ast_function_parameter& v)
        {
            n.kind = ast_node_kind::function_parameter;
            n.node.functionParameter = v;
            n.node.functionParameter.name = intern_h_str(v.name);
            n.node.functionParameter.type = intern_h_str(v.type);
        }

        void set_node(ast_node& n, const ast_function_body& v)
        {
            n.kind = ast_node_kind::function_body;
            n.node.functionBody = v;
        }

        void set_node(ast_node& n, const ast_function_call& v)
        {
            n.kind = ast_node_kind::function_call;
            n.node.functionCall = v;
            n.node.functionCall.name = intern_h_str(v.name);
        }

        void set_node(ast_node& n, const ast_function_argument& v)
        {
            n.kind = ast_node_kind::function_argument;
            n.node.functionArgument = v;
            n.node.functionArgument.name = intern_h_str(v.name);
        }

        void set_node(ast_node& n, const ast_compound& v)
        {
            n.kind = ast_node_kind::compound;
            n.node.compound = v;
        }

        void set_node(ast_node& n, const ast_binary_operator& v)
        {
            n.kind = ast_node_kind::binary_operator;
            n.node.binaryOp = v;
        }

        void set_node(ast_node& n, const ast_i32_constant& v)
        {
            n.kind = ast_node_kind::i32_constant;
            n.node.i32 = v;
        }

        void set_node(ast_node& n, const ast_u32_constant& v)
        {
            n.kind = ast_node_kind::u32_constant;
            n.node.u32 = v;
        }

        void set_node(ast_node& n, const ast_i64_constant& v)
        {
            n.kind = ast_node_kind::i64_constant;
            n.node.i64 = v;
        }

        void set_node(ast_node& n, const ast_u64_constant& v)
        {
            n.kind = ast_node_kind::u64_constant;
            n.node.u64 = v;
        }

        void set_node(ast_node& n, const ast_f32_constant& v)
        {
            n.kind = ast_node_kind::f32_constant;
            n.node.f32 = v;
        }

        void set_node(ast_node& n, const ast_f64_constant& v)
        {
            n.kind = ast_node_kind::f64_constant;
            n.node.f64 = v;
        }

        void set_node(ast_node& n, const ast_string_constant& v)
        {
            const h32 interned = m_stringInterner.get_or_add(v.value);

            n.kind = ast_node_kind::string_constant;
            n.node.string.interned = interned;
            n.node.string.value = m_stringInterner.h_str(interned);
        }

        void set_node(ast_node& n, const ast_variable_declaration& v)
        {
            n.kind = ast_node_kind::variable_declaration;
            n.node.varDecl = v;
            n.node.varDecl.name = intern_h_str(v.name);
        }

        void set_node(ast_node& n, const ast_variable_definition& v)
        {
            n.kind = ast_node_kind::variable_definition;
            n.node.varDef = v;
        }

        void set_node(ast_node& n, const ast_variable_reference& v)
        {
            n.kind = ast_node_kind::variable_reference;
            n.node.varRef = v;
            n.node.varRef.name = intern_h_str(v.name);
        }

        void set_node(ast_node& n, const ast_return_statement& v)
        {
            n.kind = ast_node_kind::return_statement;
            n.node.retStmt = v;
        }

        hashed_string_view intern_h_str(hashed_string_view str)
        {
            const h32 id = m_stringInterner.get_or_add(str);
            return m_stringInterner.h_str(id);
        }

    private:
        string_interner m_stringInterner;
        deque<ast_node> m_nodes;
    };

    template <typename T>
    h32<ast_node> abstract_syntax_tree::add_node(h32<ast_node> parent, T&& node)
    {
        const h32<ast_node> id{m_nodes.size32()};
        OBLO_ASSERT(id);

        auto& newNode = m_nodes.emplace_back();

        set_node(newNode, std::forward<T>(node));
        link_parent(parent, id);

        return id;
    }

    class abstract_syntax_tree::children_iterator
    {
    public:
        children_iterator() = default;

        children_iterator(const abstract_syntax_tree& ast, h32<ast_node> node, h32<ast_node> current) :
            m_ast{&ast}, m_node{node}, m_current{current}
        {
        }

        children_iterator(const children_iterator&) = default;
        children_iterator& operator=(const children_iterator&) = default;

        bool operator==(const children_iterator&) const = default;

        OBLO_FORCEINLINE h32<ast_node> operator*() const
        {
            return m_current;
        }

        OBLO_FORCEINLINE children_iterator operator++()
        {
            m_current = m_ast->child_next(m_node, m_current);
            return *this;
        }

        OBLO_FORCEINLINE children_iterator operator++(int)
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        OBLO_FORCEINLINE children_iterator operator--()
        {
            m_current = m_ast->child_prev(m_node, m_current);
            return *this;
        }

        OBLO_FORCEINLINE children_iterator operator--(int)
        {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

    private:
        const abstract_syntax_tree* m_ast{};
        h32<ast_node> m_node{};
        h32<ast_node> m_current{};
    };
}