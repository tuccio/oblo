#pragma once

#include <oblo/core/any.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/iterator/iterator_range.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    enum class ast_node_kind : u8
    {
        root,
        function,
        function_parameter,
        function_body,
        binary_operator,
        i64_constant,
        u64_constant,
        f32_constant,
        f64_constant,
        variable_declaration,
        variable_reference,
    };

    struct ast_root
    {
    };

    struct ast_function
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::function;

        string_view name;
        string_view returnType;
    };

    struct ast_function_parameter
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::function_parameter;

        string_view name;
        string_view type;
    };

    struct ast_function_body
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::function_body;
    };

    struct ast_binary_operator
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::binary_operator;

        string_view op;
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

    struct ast_variable_declaration
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::variable_declaration;

        string_view name;
        string_view type;
    };

    struct ast_variable_reference
    {
        static constexpr ast_node_kind node_kind = ast_node_kind::variable_reference;

        string_view name;
    };

    struct ast_node
    {
        h32<ast_node> firstChild{};
        h32<ast_node> lastChild{};
        h32<ast_node> nextSibling{};

        ast_node_kind kind{ast_node_kind::root};

        union {
            ast_root root;
            ast_function function;
            ast_function_parameter functionParameter;
            ast_function_body functionBody;
            ast_binary_operator binaryOp;
            ast_i64_constant i64;
            ast_u64_constant u64;
            ast_f32_constant f32;
            ast_f64_constant f64;
            ast_variable_declaration varDecl;
            ast_variable_reference varRef;
        } node = {.root = {}};
    };

    /// @brief Abstract syntax tree for stripped down C-like languages.
    class abstract_syntax_tree
    {
    public:
        class children_iterator;

    public:
        void init();

        h32<ast_node> get_root() const;

        template <typename T>
        h32<ast_node> add_node(h32<ast_node> parent, T&& node);

        h32<ast_node> child_next(h32<ast_node> parent, h32<ast_node> previous) const;
        iterator_range<children_iterator> children(h32<ast_node> node) const;

        const ast_node& get(h32<ast_node> node) const;
        ast_node& get(h32<ast_node> node);

    private:
        void add_child(h32<ast_node> parent, h32<ast_node> child);

        void set_node(ast_node& n, const ast_function& v)
        {
            n.kind = ast_node_kind::function;
            n.node.function = v;
        }

        void set_node(ast_node& n, const ast_function_parameter& v)
        {
            n.kind = ast_node_kind::function_parameter;
            n.node.functionParameter = v;
        }

        void set_node(ast_node& n, const ast_function_body& v)
        {
            n.kind = ast_node_kind::function_body;
            n.node.functionBody = v;
        }

        void set_node(ast_node& n, const ast_binary_operator& v)
        {
            n.kind = ast_node_kind::binary_operator;
            n.node.binaryOp = v;
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

        void set_node(ast_node& n, const ast_variable_declaration& v)
        {
            n.kind = ast_node_kind::variable_declaration;
            n.node.varDecl = v;
        }

        void set_node(ast_node& n, const ast_variable_reference& v)
        {
            n.kind = ast_node_kind::variable_reference;
            n.node.varRef = v;
        }

    private:
        deque<ast_node> m_nodes;
    };

    template <typename T>
    h32<ast_node> abstract_syntax_tree::add_node(h32<ast_node> parent, T&& node)
    {
        const h32<ast_node> id{m_nodes.size32()};
        OBLO_ASSERT(id);

        auto& newNode = m_nodes.emplace_back();

        set_node(newNode, std::forward<T>(node));
        add_child(parent, id);

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

    private:
        const abstract_syntax_tree* m_ast{};
        h32<ast_node> m_node{};
        h32<ast_node> m_current{};
    };
}