#include <oblo/nodes/node_graph.hpp>

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/graph/topological_sort.hpp>
#include <oblo/core/handle_flat_pool_set.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/overload.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/variant.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph_registry.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>

namespace oblo
{
    namespace
    {
        using node_graph_vertex_handle = node_graph::graph_type::vertex_handle;

        enum class node_flag : u8
        {
            input_changed,
            enum_max,
        };

        enum class pin_kind : u8
        {
            input,
            output,
        };

        struct node_data
        {
            // Node type id, required for serialization.
            uuid typeId{};

            // Position, required for visualization purposes, should be serialized too.
            vec2 uiPosition{};

            // The node itself.
            unique_ptr<node_interface> node;

            dynamic_array<node_graph_vertex_handle> inputPins;
            dynamic_array<node_graph_vertex_handle> outputPins;

            flags<node_flag> flags{};
        };

        struct pin_data
        {
            // Unique within the node, identifies the pin, used in serialization too.
            uuid id{};

            // The currently deduced type for the pin.
            uuid deducedType{};

            // User-readable name, only required for visualization purposes.
            string name;

            node_graph_vertex_handle ownerNode{};

            pin_kind kind;
        };

        constexpr node_graph_vertex_handle to_vertex_handle(h32<node_graph_node> h)
        {
            return node_graph_vertex_handle{h.value};
        }

        constexpr node_graph_vertex_handle to_vertex_handle(h32<node_graph_in_pin> h)
        {
            return node_graph_vertex_handle{h.value};
        }

        constexpr node_graph_vertex_handle to_vertex_handle(h32<node_graph_out_pin> h)
        {
            return node_graph_vertex_handle{h.value};
        }

        constexpr h32<node_graph_node> to_node_handle(node_graph_vertex_handle h)
        {
            return h32<node_graph_node>{h.value};
        }

        constexpr h32<node_graph_in_pin> to_in_pin_handle(node_graph_vertex_handle h)
        {
            return h32<node_graph_in_pin>{h.value};
        }

        constexpr h32<node_graph_out_pin> to_out_pin_handle(node_graph_vertex_handle h)
        {
            return h32<node_graph_out_pin>{h.value};
        }

        template <typename PinKind>
        uuid get_deduced_type_impl(const node_graph::graph_type& g, h32<PinKind> h)
        {
            OBLO_ASSERT(h);

            const pin_data& outPin = g.get(to_vertex_handle(h)).data.template as<pin_data>();
            return outPin.deducedType;
        }

        template <typename PinKind>
        void set_deduced_type_impl(node_graph::graph_type& g, h32<PinKind> h, const uuid& type)
        {
            OBLO_ASSERT(h);

            pin_data& outPin = g.get(to_vertex_handle(h)).data.template as<pin_data>();
            outPin.deducedType = type;
        }
    }

    struct node_graph::vertex_type
    {
        variant<node_data, pin_data> data;
    };

    struct node_graph::edge_type
    {
    };

    namespace
    {
        void fetch_input_pins(const node_graph::graph_type& graph,
            node_graph_vertex_handle nodeHandle,
            dynamic_array<h32<node_graph_in_pin>>& pins)
        {
            const auto& nodeVertex = graph.get(nodeHandle);
            const node_data& nodeData = nodeVertex.data.as<node_data>();

            pins.reserve_exponential(pins.size() + nodeData.inputPins.size());

            for (const h32 pin : nodeData.inputPins)
            {
                pins.emplace_back(to_in_pin_handle(pin));
            }
        }

        void fetch_output_pins(const node_graph::graph_type& graph,
            node_graph_vertex_handle nodeHandle,
            dynamic_array<h32<node_graph_out_pin>>& pins)
        {
            auto& nodeVertex = graph.get(nodeHandle);
            const node_data& nodeData = nodeVertex.data.as<node_data>();

            pins.reserve_exponential(pins.size() + nodeData.outputPins.size());

            for (const h32 pin : nodeData.outputPins)
            {
                pins.emplace_back(to_out_pin_handle(pin));
            }
        }

        template <typename T>
        h32<ast_node> create_variable_decl_or_ref(abstract_syntax_tree& ast, string_builder& buffer, h32<ast_node> expr)
        {
            const h32 parent = ast.get(expr).parent;
            buffer.clear().format("__$n{}", expr.value);
            return ast.add_node(parent, T{.name = buffer.as<hashed_string_view>()});
        }
    }

    node_graph::node_graph() = default;

    node_graph::node_graph(node_graph&& other) noexcept = default;

    node_graph& node_graph::operator=(node_graph&& other) noexcept
    {
        init(*other.m_registry);
        m_graph = std::move(other.m_graph);
        return *this;
    }

    node_graph::~node_graph() = default;

    void node_graph::init(const node_graph_registry& registry)
    {
        m_registry = &registry;
        m_graph = {};
    }

    const node_graph_registry& node_graph::get_registry() const
    {
        return *m_registry;
    }

    h32<node_graph_node> node_graph::add_node(const uuid& id)
    {
        const auto* const desc = m_registry->find_node(id);

        if (!desc)
        {
            return {};
        }

        unique_ptr nodeInstance = desc->instantiate(desc->userdata);
        auto* const nodePtr = nodeInstance.get();

        const auto nodeVertex = m_graph.add_vertex(vertex_type{
            .data =
                node_data{
                    .typeId = id,
                    .node = std::move(nodeInstance),
                },
        });

        const node_graph_context ctx{*this, nodeVertex};
        nodePtr->on_create(ctx);

        return to_node_handle(nodeVertex);
    }

    void node_graph::remove_node(h32<node_graph_node> nodeHandle)
    {
        const auto nodeVertex = to_vertex_handle(nodeHandle);

        node_data* const nodeData = m_graph.get(nodeVertex).data.try_get<node_data>();

        if (!nodeData)
        {
            return;
        }

        if (!m_graph.get(nodeVertex).data.is<node_data>())
        {
            return;
        }

        for (const auto& inPin : nodeData->inputPins)
        {
            m_graph.remove_all_edges(inPin);
            m_graph.remove_vertex(inPin);
        }

        dynamic_array<node_graph_vertex_handle> stack;
        stack.reserve(256);

        for (const auto& outPin : nodeData->outputPins)
        {
            for (const auto e : m_graph.get_out_edges(outPin))
            {
                auto* const dstNode = m_graph[e.vertex].data.try_get<node_data>();

                if (dstNode && dstNode->flags.contains(node_flag::input_changed))
                {
                    stack.emplace_back(e.vertex);
                    dstNode->flags.set(node_flag::input_changed);
                }
            }

            m_graph.remove_all_edges(outPin);
            m_graph.remove_vertex(outPin);
        }

        m_graph.remove_vertex(nodeVertex);

        call_on_input_change(stack);
    }

    void node_graph::fetch_nodes(dynamic_array<h32<node_graph_node>>& nodes) const
    {
        nodes.reserve(nodes.size() + m_graph.get_vertex_count());

        for (const h32 v : m_graph.get_vertices())
        {
            if (m_graph.get(v).data.is<node_data>())
            {
                nodes.emplace_back(to_node_handle(v));
            }
        }
    }

    void node_graph::fetch_in_pins(h32<node_graph_node> nodeHandle, dynamic_array<h32<node_graph_in_pin>>& pins) const
    {
        fetch_input_pins(m_graph, to_vertex_handle(nodeHandle), pins);
    }

    void node_graph::fetch_out_pins(h32<node_graph_node> nodeHandle, dynamic_array<h32<node_graph_out_pin>>& pins) const
    {
        fetch_output_pins(m_graph, to_vertex_handle(nodeHandle), pins);
    }

    void node_graph::fetch_properties_descriptors(h32<node_graph_node> nodeHandle,
        dynamic_array<node_property_descriptor>& outPropertyDescriptors) const
    {
        const node_data& nodeData = m_graph.get(to_vertex_handle(nodeHandle)).data.as<node_data>();
        nodeData.node->fetch_properties_descriptors(outPropertyDescriptors);
    }

    void node_graph::store(h32<node_graph_node> nodeHandle, data_document& doc, u32 docNodeIndex) const
    {
        const node_data& nodeData = m_graph.get(to_vertex_handle(nodeHandle)).data.as<node_data>();
        nodeData.node->store(doc, docNodeIndex);
    }

    void node_graph::load(h32<node_graph_node> nodeHandle, const data_document& doc, u32 docNodeIndex)
    {
        const node_data& nodeData = m_graph.get(to_vertex_handle(nodeHandle)).data.as<node_data>();
        nodeData.node->load(doc, docNodeIndex);
    }

    void node_graph::call_on_input_change(dynamic_array<node_graph_vertex_handle>& stack)
    {
        buffered_array<h32<node_graph_out_pin>, 16> outPinsBuffer;

        while (!stack.empty())
        {
            const h32 currentNodeVertex = stack.back();
            stack.pop_back();

            const node_data& currentNodeData = m_graph[currentNodeVertex].data.as<node_data>();

            currentNodeData.node->on_input_change({*this, currentNodeVertex});

            outPinsBuffer.clear();
            fetch_out_pins(to_node_handle(currentNodeVertex), outPinsBuffer);

            for (const h32 outPinHandle : outPinsBuffer)
            {
                const h32 outPinVertexHandle = to_vertex_handle(outPinHandle);
                const pin_data& outPinData = m_graph[outPinVertexHandle].data.as<pin_data>();

                auto& nextNode = m_graph[outPinData.ownerNode].data.as<node_data>();

                if (nextNode.flags.contains(node_flag::input_changed))
                {
                    nextNode.flags.unset(node_flag::input_changed);
                    stack.emplace_back(outPinData.ownerNode);
                }
            }
        }
    }

    bool node_graph::connect(h32<node_graph_out_pin> src, h32<node_graph_in_pin> dst)
    {
        const h32 srcPinVertex = to_vertex_handle(src);
        const h32 dstPinVertex = to_vertex_handle(dst);

        m_graph.add_edge(srcPinVertex, dstPinVertex);

        const pin_data& pinData = m_graph[dstPinVertex].data.as<pin_data>();

        dynamic_array<node_graph_vertex_handle> stack;
        stack.reserve(256);
        stack.emplace_back(pinData.ownerNode);

        call_on_input_change(stack);

        return true;
    }

    void node_graph::clear_connected_output(h32<node_graph_in_pin> inPin)
    {
        const auto inPinVertex = to_vertex_handle(inPin);

        // Ideally we should only have 1 incoming edge into an input, but we don't enforce it
        for (auto inEdges = m_graph.get_in_edges(inPinVertex); !inEdges.empty();
            inEdges = m_graph.get_in_edges(inPinVertex))
        {
            m_graph.remove_edge(inEdges.front().handle);
        }
    }

    h32<node_graph_out_pin> node_graph::get_connected_output(h32<node_graph_in_pin> inPin) const
    {
        const auto inPinVertex = to_vertex_handle(inPin);

        for (const auto& e : m_graph.get_in_edges(inPinVertex))
        {
            // The source should be an output pin
            OBLO_ASSERT(m_graph.get(e.vertex).data.is<pin_data>());

            if (m_graph.get(e.vertex).data.is<pin_data>())
            {
                return to_out_pin_handle(e.vertex);
            }
        }

        return {};
    }

    uuid node_graph::get_type(h32<node_graph_node> nodeHandle) const
    {
        const auto& vertexData = m_graph[to_vertex_handle(nodeHandle)].data;
        const node_data& nodeData = vertexData.as<node_data>();
        return nodeData.typeId;
    }

    const vec2& node_graph::get_ui_position(h32<node_graph_node> nodeHandle) const
    {
        const auto& vertexData = m_graph[to_vertex_handle(nodeHandle)].data;
        const node_data& nodeData = vertexData.as<node_data>();
        return nodeData.uiPosition;
    }

    void node_graph::set_ui_position(h32<node_graph_node> nodeHandle, const vec2& position)
    {
        auto& vertexData = m_graph[to_vertex_handle(nodeHandle)].data;
        node_data& nodeData = vertexData.as<node_data>();
        nodeData.uiPosition = position;
    }

    cstring_view node_graph::get_name(h32<node_graph_in_pin> pin) const
    {
        auto& vertexData = m_graph[to_vertex_handle(pin)].data;
        const pin_data& pinData = vertexData.as<pin_data>();
        return pinData.name;
    }

    cstring_view node_graph::get_name(h32<node_graph_out_pin> pin) const
    {
        auto& vertexData = m_graph[to_vertex_handle(pin)].data;
        const pin_data& pinData = vertexData.as<pin_data>();
        return pinData.name;
    }

    h32<node_graph_node> node_graph::get_owner_node(h32<node_graph_in_pin> pin) const
    {
        const auto pinVertex = to_vertex_handle(pin);
        const auto& pinData = m_graph.get(pinVertex).data.as<pin_data>();
        return to_node_handle(pinData.ownerNode);
    }

    h32<node_graph_node> node_graph::get_owner_node(h32<node_graph_out_pin> pin) const
    {
        const auto pinVertex = to_vertex_handle(pin);
        const auto& pinData = m_graph.get(pinVertex).data.as<pin_data>();
        return to_node_handle(pinData.ownerNode);
    }

    uuid node_graph::get_deduced_type(h32<node_graph_in_pin> h) const
    {
        return get_deduced_type_impl(m_graph, h);
    }

    uuid node_graph::get_deduced_type(h32<node_graph_out_pin> h) const
    {
        return get_deduced_type_impl(m_graph, h);
    }

    namespace
    {
        constexpr hashed_string_view g_DocNodesArray = "nodes"_hsv;
        constexpr hashed_string_view g_DocEdgesArray = "edges"_hsv;

        constexpr hashed_string_view g_DocNodeData = "data"_hsv;
        constexpr hashed_string_view g_DocNodeId = "id"_hsv;
        constexpr hashed_string_view g_DocNodeType = "type"_hsv;
        constexpr hashed_string_view g_DocNodeUIPosition = "uiPosition"_hsv;

        constexpr hashed_string_view g_DocEdgeSourceNode = "sourceNode"_hsv;
        constexpr hashed_string_view g_DocEdgeSourcePin = "sourcePin"_hsv;
        constexpr hashed_string_view g_DocEdgeTargetNode = "targetNode"_hsv;
        constexpr hashed_string_view g_DocEdgeTargetPin = "targetPin"_hsv;
    }

    expected<> oblo::node_graph::serialize(data_document& doc, u32 nodeIndex) const
    {
        const u32 nodesArray = doc.child_array(nodeIndex, g_DocNodesArray);
        const u32 edgesArray = doc.child_array(nodeIndex, g_DocEdgesArray);

        for (const h32 v : m_graph.get_vertices())
        {
            m_graph[v].data.visit(overload{
                [this, &doc, v, nodesArray](const node_data& node)
                {
                    const u32 nodeObj = doc.array_push_back(nodesArray);
                    doc.make_object(nodeObj);

                    doc.child_value(nodeObj, g_DocNodeId, property_value_wrapper{v.value});
                    doc.child_value(nodeObj, g_DocNodeType, property_value_wrapper{node.typeId});

                    const u32 uiPosition = doc.child_array(nodeObj, g_DocNodeUIPosition, 2);
                    const u32 uiPositionX = doc.child_next(uiPosition, data_node::Invalid);
                    const u32 uiPositionY = doc.child_next(uiPosition, uiPositionX);

                    doc.make_value(uiPositionX, property_value_wrapper{node.uiPosition.x});
                    doc.make_value(uiPositionY, property_value_wrapper{node.uiPosition.y});

                    const u32 dataNodeIndex = doc.child_object(nodeObj, g_DocNodeData);
                    store(to_node_handle(v), doc, dataNodeIndex);
                },
                [this, &doc, v, edgesArray](const pin_data& srcPin)
                {
                    // We visit output pins only, write down edges from the owner to the destination's owner
                    if (srcPin.kind == pin_kind::input)
                    {
                        return;
                    }

                    const h32 srcVertex = srcPin.ownerNode;

                    for (auto& e : m_graph.get_out_edges(v))
                    {
                        const pin_data* const dstPin = m_graph[e.vertex].data.try_get<pin_data>();

                        if (!dstPin)
                        {
                            OBLO_ASSERT(dstPin);
                            continue;
                        }

                        OBLO_ASSERT(dstPin->kind == pin_kind::input);
                        const h32 targetVertex = dstPin->ownerNode;

                        const u32 edgeObj = doc.array_push_back(edgesArray);
                        doc.make_object(edgeObj);

                        doc.child_value(edgeObj, g_DocEdgeSourceNode, property_value_wrapper{srcVertex.value});
                        doc.child_value(edgeObj, g_DocEdgeTargetNode, property_value_wrapper{targetVertex.value});
                        doc.child_value(edgeObj, g_DocEdgeSourcePin, property_value_wrapper{srcPin.id});
                        doc.child_value(edgeObj, g_DocEdgeTargetPin, property_value_wrapper{dstPin->id});
                    }
                },
            });
        }

        return no_error;
    }

    expected<> oblo::node_graph::deserialize(const data_document& doc, u32 nodeIndex)
    {
        const u32 nodesArray = doc.find_child(nodeIndex, g_DocNodesArray);
        const u32 edgesArray = doc.find_child(nodeIndex, g_DocEdgesArray);

        if (nodesArray == data_node::Invalid || edgesArray == data_node::Invalid)
        {
            return unspecified_error;
        }

        bool anyNodeFailed = false;
        bool anyEdgeFailed = false;

        std::unordered_map<u32, h32<node_graph_node>> idToNode;

        for (const u32 nodeObj : doc.children(nodesArray))
        {
            const u32 nodeTypeIndex = doc.find_child(nodeObj, g_DocNodeType);
            const u32 nodeIdIndex = doc.find_child(nodeObj, g_DocNodeId);
            const u32 nodeUiPosIndex = doc.find_child(nodeObj, g_DocNodeUIPosition);
            const u32 nodeDataIndex = doc.find_child(nodeObj, g_DocNodeData);

            if (nodeTypeIndex == data_node::Invalid || nodeIdIndex == data_node::Invalid)
            {
                anyNodeFailed = true;
                continue;
            }

            const auto typeId = doc.read_uuid(nodeTypeIndex);

            if (!typeId)
            {
                anyNodeFailed = true;
                continue;
            }

            const h32 handle = add_node(*typeId);

            if (!handle)
            {
                anyNodeFailed = true;
                continue;
            }

            const auto id = doc.read_u32(nodeIdIndex);

            if (id)
            {
                idToNode.emplace(*id, handle);
            }

            node_data& node = m_graph[to_vertex_handle(handle)].data.as<node_data>();

            if (doc.children_count(nodeUiPosIndex) == 2)
            {
                u32 i = 0;

                for (const u32 posIndex : doc.children(nodeUiPosIndex))
                {
                    node.uiPosition[i] = doc.read_f32(posIndex).value_or(0.f);
                    ++i;

                    if (i > 2) [[unlikely]]
                    {
                        OBLO_ASSERT(false);
                        break;
                    }
                }
            }

            if (nodeDataIndex != data_node::Invalid)
            {
                load(handle, doc, nodeDataIndex);
            }
        }

        for (const u32 edgeObj : doc.children(edgesArray))
        {
            const u32 edgeSrcNode = doc.find_child(edgeObj, g_DocEdgeSourceNode);
            const u32 edgeSrcPin = doc.find_child(edgeObj, g_DocEdgeSourcePin);
            const u32 edgeTargetNode = doc.find_child(edgeObj, g_DocEdgeTargetNode);
            const u32 edgeTargetPin = doc.find_child(edgeObj, g_DocEdgeTargetPin);

            if (edgeSrcNode == data_node::Invalid || edgeSrcPin == data_node::Invalid ||
                edgeTargetNode == data_node::Invalid || edgeTargetPin == data_node::Invalid)
            {
                anyEdgeFailed = true;
                continue;
            }

            const auto srcId = doc.read_u32(edgeSrcNode);
            const auto dstId = doc.read_u32(edgeTargetNode);
            const auto srcPin = doc.read_uuid(edgeSrcPin);
            const auto dstPin = doc.read_uuid(edgeTargetPin);

            if (!srcId || !dstId || !srcPin || !dstPin)
            {
                anyEdgeFailed = true;
                continue;
            }

            const auto srcNodeIt = idToNode.find(*srcId);
            const auto dstNodeIt = idToNode.find(*dstId);

            if (srcNodeIt == idToNode.end() || dstNodeIt == idToNode.end())
            {
                anyEdgeFailed = true;
                continue;
            }

            const h32 srcHandle = to_vertex_handle(srcNodeIt->second);
            const h32 dstHandle = to_vertex_handle(dstNodeIt->second);

            const node_data& srcNode = m_graph[srcHandle].data.as<node_data>();
            const node_data& dstNode = m_graph[dstHandle].data.as<node_data>();

            const auto findPinById = [this](std::span<const node_graph_vertex_handle> pins,
                                         const uuid& id) -> node_graph_vertex_handle
            {
                for (const h32 pin : pins)
                {
                    if (m_graph[pin].data.as<pin_data>().id == id)
                    {
                        return pin;
                    }
                }

                return {};
            };

            const h32 srcPinHandle = findPinById(srcNode.outputPins, *srcPin);
            const h32 dstPinHandle = findPinById(dstNode.inputPins, *dstPin);

            anyEdgeFailed |= !connect(to_out_pin_handle(srcPinHandle), to_in_pin_handle(dstPinHandle));
        }

        if (anyNodeFailed)
        {
            return unspecified_error;
        }

        if (anyEdgeFailed)
        {
            return unspecified_error;
        }

        return no_error;
    }

    expected<> node_graph::generate_ast(abstract_syntax_tree& ast) const
    {
        if (!ast.is_initialized())
        {
            ast.init();
        }

        // INPUT first might be better, e.g. say we have i32 and f32 constant and binary add, the binary add wants to
        // check the type of the inputs and convert them this is NOT per node, e.g. an i32 constant might be connected
        // to a binary add that needs promotion and one that does not

        // topological sort
        // Optional: Mark all nodes that lead to an output
        //
        // If node: is any output pin connected?
        //  - No: skip
        //  - Yes:
        //    - Gather all inputs, if one input is variable-set expression, add variable-ref expression, otherwise use
        //    as-is
        //    - Generate node with expressions for all output pins (it should need input expressions), add all
        //    expressions under root (or maybe under "unused" dummy, so we can cull unused expressions later?)
        //      - This depends on how we want to generate, do we want to look at input expressions or should
        //      node_interface::generate look at input pins by itself?
        //    - IMPORTANT: Check that node::generate reparented the inputs?
        // If output pin:
        //  - If node was skipped or pin is not connected, skip
        //  - If pin is connected to 1 pin that's the expression
        //  - If connected to more than one, add variable-set expression and push it to tree under parent

        string_builder builderBuffer;

        dynamic_array<node_graph_vertex_handle> sortedVertices;

        const bool wasSorted = topological_sort(m_graph, sortedVertices);

        if (!wasSorted)
        {
            return unspecified_error;
        }

        const h32 root = ast.get_root();

        const h32 executeDecl = ast.add_node(root,
            ast_function_declaration{
                .name = "node_graph_execute",
                .returnType = "void",
            });

        const h32 functionBody = ast.add_node(executeDecl, ast_function_body{});

        // We use a compound for statements we need to execute before the rest (e.g. variables we store because they are
        // used by multiple nodes)
        const h32 executeStatements = ast.add_node(functionBody, ast_compound{});
        const h32 preExecuteStatements = ast.add_node(functionBody, ast_compound{});

        struct output_pin_data
        {
            h32<ast_node> expression;
            h32<ast_node> varDecl;
        };

        h32_flat_extpool_dense_map<node_graph_vertex_handle::tag_type, output_pin_data> outputPins;

        dynamic_array<h32<ast_node>> inputs;
        dynamic_array<h32<ast_node>> outputs;

        inputs.reserve(32);
        outputs.reserve(32);

        for (const h32 currentVertex : reverse_range(sortedVertices))
        {
            auto& vertexData = m_graph[currentVertex].data;

            if (vertexData.is<pin_data>())
            {
                const pin_data& pin = m_graph[currentVertex].data.as<pin_data>();

                if (pin.kind == pin_kind::output)
                {
                    auto* const outPin = outputPins.try_find(currentVertex);

                    // When we find output pins, they should have already been generated
                    OBLO_ASSERT(outPin && outPin->expression && !outPin->varDecl);

                    if (!outPin || !outPin->expression || outPin->varDecl)
                    {
                        return unspecified_error;
                    }

                    // If they are only connected to 1 pin, we already have our expression.
                    // Otherwise we create a variable definition to put the value in.
                    // Nodes that consume the pin should then make variable references instead.
                    if (m_graph.get_out_edges(currentVertex).size() >= 2)
                    {
                        const h32 decl = create_variable_decl_or_ref<ast_variable_declaration>(ast,
                            builderBuffer,
                            outPin->expression);

                        const h32 def = ast.add_node(decl, ast_variable_definition{});

                        ast.reparent(outPin->expression, def);

                        outPin->varDecl = decl;
                    }
                }
            }
            else if (vertexData.is<node_data>())
            {
                const node_data& node = m_graph[currentVertex].data.as<node_data>();

                inputs.clear();
                outputs.clear();

                for (const h32 inPin : node.inputPins)
                {
                    auto& inputAstNode = inputs.emplace_back();

                    for (const auto& edge : m_graph.get_in_edges(inPin))
                    {
                        if (!m_graph[edge.vertex].data.is<pin_data>())
                        {
                            continue;
                        }

                        auto* const astNode = outputPins.try_find(edge.vertex);

                        if (!astNode || !astNode->expression)
                        {
                            return unspecified_error;
                        }

                        if (astNode->varDecl)
                        {
                            // This means that the node was referenced by more than one input, so we reference the
                            // variable.
                            inputAstNode = create_variable_decl_or_ref<ast_variable_reference>(ast,
                                builderBuffer,
                                astNode->expression);

                            // We lazily reparent the statements when they are used
                            // We need to preserve the topological order when reparenting
                            if (ast.get_parent(astNode->varDecl) != preExecuteStatements)
                            {
                                ast.reparent_first(astNode->varDecl, preExecuteStatements);
                            }
                        }
                        else
                        {
                            inputAstNode = astNode->expression;
                        }
                    }
                }

                // TODO: Parametrize graph context for const/non-const
                const node_graph_context ctx{*const_cast<node_graph*>(this), currentVertex};

                if (!node.node->generate(ctx, ast, executeStatements, inputs, outputs))
                {
                    return unspecified_error;
                }

                if (node.outputPins.size() != node.outputPins.size())
                {
                    return unspecified_error;
                }

                // Store the generated AST node for each output pin, they will be used to feed the connected inputs
                for (const auto [outPin, astNode] : zip_range(node.outputPins, outputs))
                {
                    outputPins.emplace(outPin, astNode);
                }
            }
        }

        return no_error;
    }

    node_graph_context::node_graph_context(node_graph& g, node_graph_vertex_handle node) :
        m_registry{g.m_registry}, m_graph{&g.m_graph}, m_node{node}
    {
    }

    h32<node_graph_in_pin> node_graph_context::add_in_pin(const pin_descriptor& desc) const
    {
        const auto pinVertex = m_graph->add_vertex(node_graph::vertex_type{
            .data =
                pin_data{
                    .id = desc.id,
                    .name = desc.name,
                    .ownerNode = m_node,
                    .kind = pin_kind::input,
                },
        });

        auto& nodeVertex = m_graph->get(m_node);
        node_data& nodeData = nodeVertex.data.as<node_data>();
        nodeData.inputPins.emplace_back(pinVertex);

        // Add edge from input pin to vertex to ensure topological order
        m_graph->add_edge(pinVertex, m_node);

        return to_in_pin_handle(pinVertex);
    }

    h32<node_graph_out_pin> node_graph_context::add_out_pin(const pin_descriptor& desc) const
    {
        const auto pinVertex = m_graph->add_vertex(node_graph::vertex_type{
            .data =
                pin_data{
                    .id = desc.id,
                    .name = desc.name,
                    .ownerNode = m_node,
                    .kind = pin_kind::output,
                },
        });

        auto& nodeVertex = m_graph->get(m_node);
        node_data& nodeData = nodeVertex.data.as<node_data>();
        nodeData.outputPins.emplace_back(pinVertex);

        // Add edge from vertex to output pin to ensure topological order
        m_graph->add_edge(m_node, pinVertex);

        return to_out_pin_handle(pinVertex);
    }

    void node_graph_context::fetch_in_pins(dynamic_array<h32<node_graph_in_pin>>& pins) const
    {
        fetch_input_pins(*m_graph, m_node, pins);
    }

    void node_graph_context::fetch_out_pins(dynamic_array<h32<node_graph_out_pin>>& pins) const
    {
        fetch_output_pins(*m_graph, m_node, pins);
    }

    void node_graph_context::mark_modified(h32<node_graph_out_pin> h) const
    {
        for (const auto e : m_graph->get_out_edges(to_vertex_handle(h)))
        {
            const auto dstPinVertex = e.vertex;
            const pin_data& dstPinData = m_graph->get(dstPinVertex).data.as<pin_data>();
            node_data& dstNodeData = m_graph->get(dstPinData.ownerNode).data.as<node_data>();

            dstNodeData.flags.set(node_flag::input_changed);
        }
    }

    uuid node_graph_context::get_incoming_type(h32<node_graph_in_pin> h) const
    {
        OBLO_ASSERT(h);

        const std::span inEdges = m_graph->get_in_edges(to_vertex_handle(h));

        if (inEdges.size() != 1)
        {
            return {};
        }

        const h32 sourcePin = inEdges[0].vertex;

        return get_deduced_type(to_out_pin_handle(sourcePin));
    }

    uuid node_graph_context::get_deduced_type(h32<node_graph_in_pin> h) const
    {
        return get_deduced_type_impl(*m_graph, h);
    }

    void node_graph_context::set_deduced_type(h32<node_graph_in_pin> h, const uuid& type) const
    {
        return set_deduced_type_impl(*m_graph, h, type);
    }

    uuid node_graph_context::get_deduced_type(h32<node_graph_out_pin> h) const
    {
        return get_deduced_type_impl(*m_graph, h);
    }

    void node_graph_context::set_deduced_type(h32<node_graph_out_pin> h, const uuid& type) const
    {
        return set_deduced_type_impl(*m_graph, h, type);
    }

    uuid node_graph_context::find_promotion_rule(const uuid& lhs, const uuid& rhs) const
    {
        return m_registry->find_promotion_rule(lhs, rhs);
    }
}