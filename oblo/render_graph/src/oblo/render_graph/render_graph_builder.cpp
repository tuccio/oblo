#include <oblo/render_graph/render_graph.hpp>
#include <oblo/render_graph/render_graph_builder.hpp>

#include <memory>

namespace oblo
{
    render_graph render_graph_builder::build() const
    {
        render_graph graph;

        usize maxNodeSize{0};
        usize maxOutputsSize{0};

        constexpr auto maxTypeSize = [](const auto& type) { return type.size + type.alignment - 1; };

        for (const auto& pin : m_broadcastPins)
        {
            maxOutputsSize += maxTypeSize(pin);
        }

        for (const auto& [typeId, nodeType] : m_nodeTypes)
        {
            maxNodeSize += maxTypeSize(nodeType);

            for (auto i = nodeType.outputsBegin; i != nodeType.outputsEnd; ++i)
            {
                maxOutputsSize += maxTypeSize(m_pins[i]);
            }
        }

        // TODO: Could consider a different approach to shrink the allocation, but it doesn't matter too much for now
        auto& nodes = graph.m_nodes;
        auto& inputs = graph.m_inputs;
        auto& nodesStorage = graph.m_nodeStorage;
        auto& pinsStorage = graph.m_pinStorage;

        nodes.reserve(m_nodeTypes.size());
        inputs.reserve(m_broadcastPins.size());
        nodesStorage.resize(maxNodeSize);
        pinsStorage.resize(maxOutputsSize);

        void* nextNodePtr = nodesStorage.data();
        void* nextPinPtr = pinsStorage.data();

        for (const auto& pin : m_broadcastPins)
        {
            auto* const pinPtr = std::align(pin.alignment, pin.size, nextPinPtr, maxOutputsSize);
            nextPinPtr = static_cast<std::byte*>(nextPinPtr) + pin.size;

            pin.initialize(pinPtr);

            inputs.push_back({.ptr = pinPtr, .typeId = pin.typeId, .name = std::string{pin.name}});
        }

        // Initialize the nodes and the storage for pins
        for (const auto& [typeId, nodeType] : m_nodeTypes)
        {
            auto* const nodePtr = std::align(nodeType.alignment, nodeType.size, nextNodePtr, maxNodeSize);
            nextNodePtr = static_cast<std::byte*>(nextNodePtr) + nodeType.size;

            nodeType.initialize(nodePtr);
            graph.m_nodes.push_back({
                .ptr = nodePtr,
                .typeId = typeId,
                .execute = nodeType.execute,
            });

            // We only store output pins, input pins will reference the connected output
            for (auto i = nodeType.outputsBegin; i != nodeType.outputsEnd; ++i)
            {
                auto& outPin = m_pins[i];
                auto* const pinPtr = std::align(outPin.alignment, outPin.size, nextPinPtr, maxOutputsSize);
                nextPinPtr = static_cast<std::byte*>(nextPinPtr) + outPin.size;

                outPin.initialize(pinPtr);

                auto* const outPinMemberPtr = static_cast<std::byte*>(nodePtr) + outPin.offset;
                std::memcpy(outPinMemberPtr, &pinPtr, sizeof(void*));
            }

            for (auto i = nodeType.inputsBegin; i != nodeType.inputsEnd; ++i)
            {
                auto& inPin = m_pins[i];

                if (inPin.nextConnectedPin == Invalid)
                {
                    const auto it = std::find_if(m_broadcastPins.begin(),
                                                 m_broadcastPins.end(),
                                                 [&inPin](const node_pin& pin)
                                                 { return pin.name == inPin.name && pin.typeId == inPin.typeId; });

                    // TODO: Proper error handling
                    OBLO_ASSERT(it != m_broadcastPins.end());
                    const auto inputPinIndex = narrow_cast<u32>(std::addressof(*it) - m_broadcastPins.data());

                    auto* const outPinMemberPtr = static_cast<std::byte*>(nodePtr) + inPin.offset;
                    std::memcpy(outPinMemberPtr, &inputs[inputPinIndex], sizeof(void*));
                }
            }
        }

        // Once all nodes have been initialized, we can also connect all pointers for input pins
        for (const auto& [typeId, nodeType] : m_nodeTypes)
        {
            for (auto i = nodeType.outputsBegin; i != nodeType.outputsEnd; ++i)
            {
                auto& outPin = m_pins[i];
                auto* const nodePtr = graph.m_nodes[outPin.nodeIndex].ptr;

                auto* const outPinMemberPtr = static_cast<std::byte*>(nodePtr) + outPin.offset;

                for (auto i = outPin.nextConnectedPin; i != Invalid;)
                {
                    auto& inPin = m_pins[i];
                    auto* const connectedNode = graph.m_nodes[inPin.nodeIndex].ptr;
                    auto* const inPinMemberPtr = static_cast<std::byte*>(connectedNode) + inPin.offset;
                    std::memcpy(inPinMemberPtr, outPinMemberPtr, sizeof(void*));
                    i = inPin.nextConnectedPin;
                }
            }
        }

        return graph;
    }

    void render_graph_builder::add_edge_impl(
        node_type& nodeFrom, std::string_view pinFrom, node_type& nodeTo, std::string_view pinTo, type_id type)
    {
        const auto from = std::span{m_pins}.subspan(nodeFrom.outputsBegin, nodeFrom.outputsEnd - nodeFrom.outputsBegin);
        const auto to = std::span{m_pins}.subspan(nodeTo.inputsBegin, nodeTo.inputsEnd - nodeTo.inputsBegin);

        const auto itFrom =
            std::find_if(from.begin(),
                         from.end(),
                         [pinFrom, type](const node_pin& pin) { return pin.name == pinFrom && pin.typeId == type; });

        const auto itTo =
            std::find_if(to.begin(),
                         to.end(),
                         [pinTo, type](const node_pin& pin) { return pin.name == pinTo && pin.typeId == type; });

        OBLO_ASSERT(itFrom != m_inputs.end() && itTo != m_outputs.end());
        OBLO_ASSERT(itTo->nextConnectedPin == Invalid);

        const auto inputPinIndex = narrow_cast<u32>(std::addressof(*itTo) - m_pins.data());

        // We keep a linked list of all the inputs attached to the same output
        itTo->nextConnectedPin = itFrom->nextConnectedPin;
        itFrom->nextConnectedPin = inputPinIndex;
    }
}