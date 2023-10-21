// #include <oblo/vulkan/graph/render_graph_builder.hpp>

// #include <oblo/vulkan/graph/render_graph.hpp>
// #include <oblo/vulkan/graph/render_graph_seq_executor.hpp>

// #include <memory>

// namespace oblo::vk
// {
//     std::error_code render_graph_builder_impl::build(render_graph& graph, render_graph_seq_executor& executor) const
//     {
//         const auto ec = build_graph(graph);
//         return ec ? ec : build_executor(graph, executor);
//     }

//     std::error_code render_graph_builder_impl::build_graph(render_graph& graph) const
//     {
//         if (m_lastError)
//         {
//             return m_lastError;
//         }

//         usize maxNodeSize{0};
//         usize maxOutputsSize{0};

//         constexpr auto maxTypeSize = [](const auto& type) { return type.size + type.alignment - 1; };

//         for (const auto& pin : m_broadcastPins)
//         {
//             maxOutputsSize += maxTypeSize(pin);
//         }

//         for (const auto& [typeId, nodeType] : m_nodeTypes)
//         {
//             maxNodeSize += maxTypeSize(nodeType);

//             for (auto i = nodeType.outputsBegin; i != nodeType.outputsEnd; ++i)
//             {
//                 maxOutputsSize += maxTypeSize(m_pins[i]);
//             }
//         }

//         // TODO: Could consider a different approach to shrink the allocation, but it doesn't matter too much for now
//         auto& nodes = graph.m_nodes;
//         auto& inputs = graph.m_inputs;
//         auto& nodesStorage = graph.m_nodeStorage;
//         auto& pinsStorage = graph.m_pinStorage;

//         nodes.reserve(m_nodeTypes.size());
//         inputs.reserve(m_broadcastPins.size());
//         nodesStorage.resize(maxNodeSize);
//         pinsStorage.resize(maxOutputsSize);

//         void* nextNodePtr = nodesStorage.data();
//         void* nextPinPtr = pinsStorage.data();

//         for (const auto& pin : m_broadcastPins)
//         {
//             auto* const pinPtr = std::align(pin.alignment, pin.size, nextPinPtr, maxOutputsSize);
//             nextPinPtr = static_cast<std::byte*>(nextPinPtr) + pin.size;

//             pin.construct(pinPtr);

//             inputs.push_back({.ptr = pinPtr, .typeId = pin.typeId, .name = std::string{pin.name}});
//         }

//         // Initialize the nodes and the storage for pins
//         for (const auto& [typeId, nodeType] : m_nodeTypes)
//         {
//             auto* const nodePtr = std::align(nodeType.alignment, nodeType.size, nextNodePtr, maxNodeSize);
//             nextNodePtr = static_cast<std::byte*>(nextNodePtr) + nodeType.size;

//             nodeType.construct(nodePtr);
//             graph.m_nodes.push_back({.ptr = nodePtr, .typeId = typeId});

//             // We only store output pins, input pins will reference the connected output
//             for (auto i = nodeType.outputsBegin; i != nodeType.outputsEnd; ++i)
//             {
//                 auto& outPin = m_pins[i];
//                 auto* const pinPtr = std::align(outPin.alignment, outPin.size, nextPinPtr, maxOutputsSize);
//                 nextPinPtr = static_cast<std::byte*>(nextPinPtr) + outPin.size;

//                 outPin.construct(pinPtr);

//                 auto* const outPinMemberPtr = static_cast<std::byte*>(nodePtr) + outPin.offset;
//                 std::memcpy(outPinMemberPtr, &pinPtr, sizeof(void*));
//             }

//             for (auto i = nodeType.inputsBegin; i != nodeType.inputsEnd; ++i)
//             {
//                 auto& inPin = m_pins[i];

//                 if (inPin.connectedOutput == Invalid)
//                 {
//                     const auto it = std::find_if(m_broadcastPins.begin(),
//                                                  m_broadcastPins.end(),
//                                                  [&inPin](const node_pin& pin)
//                                                  { return pin.name == inPin.name && pin.typeId == inPin.typeId; });

//                     if (it == m_broadcastPins.end())
//                     {
//                         return make_error_code(render_graph_builder_error::missing_input);
//                     }

//                     const auto inputPinIndex = narrow_cast<u32>(std::addressof(*it) - m_broadcastPins.data());

//                     auto* const outPinMemberPtr =
//                         static_cast<std::byte*>(nodePtr) + inPin.offset + inPin.connectionOffset;
//                     std::memcpy(outPinMemberPtr, &inputs[inputPinIndex], sizeof(void*));
//                 }
//             }
//         }

//         const auto connectToInputs = [&graph, this](const node_pin& outPin, void* outPinMemberPtr)
//         {
//             for (auto connectedInputIndex = outPin.nextConnectedInput; connectedInputIndex != Invalid;)
//             {
//                 auto& inPin = m_pins[connectedInputIndex];
//                 auto* const connectedNode = graph.m_nodes[inPin.nodeIndex].ptr;
//                 auto* const inPinMemberPtr = static_cast<std::byte*>(connectedNode) + inPin.offset;
//                 std::memcpy(inPinMemberPtr, outPinMemberPtr, sizeof(void*));
//                 connectedInputIndex = inPin.nextConnectedInput;
//             }
//         };

//         // Once all nodes have been initialized, we can also connect all pointers for input pins
//         for (const auto& [typeId, nodeType] : m_nodeTypes)
//         {
//             for (auto outputIndex = nodeType.outputsBegin; outputIndex != nodeType.outputsEnd; ++outputIndex)
//             {
//                 auto& outPin = m_pins[outputIndex];
//                 auto* const outPinMemberPtr =
//                     static_cast<std::byte*>(graph.m_nodes[outPin.nodeIndex].ptr) + outPin.offset;
//                 connectToInputs(outPin, outPinMemberPtr);
//             }
//         }

//         OBLO_ASSERT(m_broadcastPins.size() == graph.m_inputs.size());
//         for (usize inputIndex = 0; inputIndex < m_broadcastPins.size(); ++inputIndex)
//         {
//             connectToInputs(m_broadcastPins[inputIndex], &graph.m_inputs[inputIndex].ptr);
//         }

//         return {};
//     }

//     std::error_code render_graph_builder_impl::build_executor(const render_graph& graph,
//                                                               render_graph_seq_executor& executor) const
//     {
//         if (m_lastError)
//         {
//             return m_lastError;
//         }

//         (void) graph;
//         (void) executor;

//         // enum class visit_state : u8
//         // {
//         //     unvisited,
//         //     visiting,
//         //     visited
//         // };

//         // const usize numNodes = m_nodeTypes.size();
//         // std::vector<visit_state> nodeStates{numNodes, visit_state::unvisited};

//         // auto& executorNodes = executor.m_nodes;
//         // executorNodes.resize(numNodes);

//         // auto nextOutputIt = executorNodes.rbegin();

//         // // Recursive function for topological sort based on DFS visit
//         // const auto dfs = [this, &nodeStates, &graphNodes = graph.m_nodes, &nextOutputIt](auto&& recurse,
//         //                                                                                  u32 i) -> std::error_code
//         // {
//         //     auto& nodeState = nodeStates[i];

//         //     if (nodeState == visit_state::visited)
//         //     {
//         //         return {};
//         //     }

//         //     if (nodeState == visit_state::visiting)
//         //     {
//         //         return render_graph_builder_error::not_a_dag;
//         //     }

//         //     auto& graphNode = graphNodes[i];
//         //     auto& nodeType = m_nodeTypes.at(graphNode.typeId);

//         //     const auto outputPins =
//         //         std::span{m_pins}.subspan(nodeType.outputsBegin, nodeType.outputsEnd - nodeType.outputsBegin);

//         //     nodeState = visit_state::visiting;

//         //     for (const auto& outPin : outputPins)
//         //     {
//         //         for (auto connectedInput = outPin.nextConnectedInput; connectedInput != Invalid;)
//         //         {
//         //             auto& inPin = m_pins[connectedInput];
//         //             recurse(recurse, inPin.nodeIndex);
//         //             connectedInput = inPin.nextConnectedInput;
//         //         }
//         //     }

//         //     *nextOutputIt = {
//         //         .ptr = graphNode.ptr,
//         //         .initialize = nodeType.initialize,
//         //         .execute = nodeType.execute,
//         //         .shutdown = nodeType.shutdown,
//         //     };

//         //     ++nextOutputIt;

//         //     nodeState = visit_state::visited;
//         //     return {};
//         // };

//         // for (u32 i = 0; i < numNodes; ++i)
//         // {
//         //     if (const auto ec = dfs(dfs, i))
//         //     {
//         //         return ec;
//         //     }
//         // }

//         return {};
//     }

//     namespace
//     {
//         class render_graph_builder_error_category final : public std::error_category
//         {
//             const char* name() const noexcept final
//             {
//                 return "render_graph_builder_error_category";
//             }

//             std::string message(int condition) const final
//             {
//                 switch (narrow_cast<render_graph_builder_error>(condition))
//                 {

//                 case render_graph_builder_error::success:
//                     return "Success";
//                 case render_graph_builder_error::node_not_found:
//                     return "The specified node was not found";
//                 case render_graph_builder_error::pin_not_found:
//                     return "The specified pin was not found";
//                 case render_graph_builder_error::missing_input:
//                     return "An input pin was not connected";
//                 case render_graph_builder_error::input_already_connected:
//                     return "The specified input is already connected";
//                 case render_graph_builder_error::not_a_dag:
//                     return "The graph contains a cycle";
//                 default:
//                     return "Unknown error";
//                 }
//             }
//         };

//         static const render_graph_builder_error_category g_category;
//     }

//     const std::error_category& render_graph_builder_impl::error_category()
//     {
//         return g_category;
//     }
// }