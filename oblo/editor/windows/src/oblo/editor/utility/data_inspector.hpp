#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/overload.hpp>
#include <oblo/editor/ui/property_table.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/properties/attributes.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/visit.hpp>
#include <oblo/reflection/concepts/resource_type.hpp>
#include <oblo/reflection/reflection_registry.hpp>

#include <imgui.h>

namespace oblo
{
    struct linear_color_tag;
}

namespace oblo::editor
{
    class data_inspector
    {
    public:
        void init(const reflection::reflection_registry* reflection, ui::artifact_picker* picker)
        {
            m_reflection = reflection;
            m_artifactPicker = picker;
        }

        void begin()
        {
            m_stringBuffers.clear();
        }

        void end() {}

        bool build_property_table(const property_tree& tree, std::byte* const data)
        {
            bool modified = false;

            auto* ptr = data;

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            usize nextStringBufferIdx = 0;

            if (ui::property_table::begin())
            {
                visit(tree,
                    overload{
                        [this, &ptr, &tree, &modified](const property_node& node, const property_node_start)
                        {
                            ptr += node.offset;

                            if (node.type == get_type_id<vec3>())
                            {
                                if (find_attribute<linear_color_tag>(tree, node))
                                {
                                    modified |= build_linear_color_editor(node, ptr);
                                    return visit_result::sibling;
                                }

                                modified |= build_vec3_editor(node, ptr);
                                return visit_result::sibling;
                            }

                            if (node.type == get_type_id<quaternion>())
                            {
                                modified |= build_quaternion_editor(node, ptr);
                                return visit_result::sibling;
                            }

                            if (node.type == get_type_id<mat4>())
                            {
                                auto* const v = new (ptr) mat4;
                                modified |= ui::property_table::add(int(hash_mix(node.offset, 0)), node.name, *v);
                                return visit_result::sibling;
                            }

                            if (node.type == get_type_id<radians>())
                            {
                                auto* const r = new (ptr) radians;
                                modified |= ui::property_table::add(int(hash_mix(node.offset, 0)), node.name, *r);
                                return visit_result::sibling;
                            }

                            if (node.type == get_type_id<degrees>())
                            {
                                auto* const r = new (ptr) degrees;
                                modified |= ui::property_table::add(int(hash_mix(node.offset, 0)), node.name, *r);
                                return visit_result::sibling;
                            }

                            return visit_result::recurse;
                        },
                        [&ptr](const property_node& node, const property_node_finish) { ptr -= node.offset; },
                        [](const property_node&, const property_array&, auto&&) { return visit_result::sibling; },
                        [this, &ptr, &tree, &modified, &nextStringBufferIdx](const property& property)
                        {
                            const auto makeId = [&property]() -> ui::property_table::id_t
                            {
                                struct hash_data
                                {
                                    u32 stackTop;
                                    u32 offset;
                                    u32 parent;
                                };

                                const hash_data h{
                                    .stackTop = ImGui::GetItemID(),
                                    .offset = property.offset,
                                    .parent = property.parent,
                                };

                                return std::bit_cast<ui::property_table::id_t>(hash_xxh32(&h, sizeof(h)));
                            };

                            byte* const propertyPtr = ptr + property.offset;

                            if (property.isEnum)
                            {
                                modified |= ui::property_table::add_enum(makeId(),
                                    property.name,
                                    propertyPtr,
                                    property.type,
                                    *m_reflection);

                                return visit_result::recurse;
                            }

                            switch (property.kind)
                            {
                            case property_kind::f32:
                                modified |= ui::property_table::add(makeId(), property.name, *new (propertyPtr) f32);
                                break;

                            case property_kind::u32:
                                modified |= ui::property_table::add(makeId(), property.name, *new (propertyPtr) u32);
                                break;

                            case property_kind::boolean:
                                modified |= ui::property_table::add(makeId(), property.name, *new (propertyPtr) bool);
                                break;

                            case property_kind::uuid: {
                                const auto parentType = m_reflection->find_type(tree.nodes[property.parent].type);

                                if (const auto resourceRef =
                                        m_reflection->find_concept<reflection::resource_type>(parentType);
                                    m_artifactPicker && resourceRef)
                                {
                                    modified |= ui::property_table::add(makeId(),
                                        tree.nodes[property.parent].name,
                                        *new (propertyPtr) uuid,
                                        *m_artifactPicker,
                                        resourceRef->typeUuid);
                                }
                                else
                                {
                                    modified |=
                                        ui::property_table::add(makeId(), property.name, *new (propertyPtr) uuid);
                                }
                            }

                            break;

                            case property_kind::string: {
                                property_value_wrapper srcWrapper;
                                srcWrapper.assign_from(property.kind, propertyPtr);
                                const auto src = srcWrapper.get_string();

                                if (nextStringBufferIdx >= m_stringBuffers.size())
                                {
                                    m_stringBuffers.resize(nextStringBufferIdx + 1);
                                }

                                auto& stringBuffer = m_stringBuffers[nextStringBufferIdx];
                                ++nextStringBufferIdx;

                                const auto numChars = min(src.size(), sizeof(string_buffer) - 1);
                                std::memcpy(stringBuffer.buffer, src.data(), numChars);
                                stringBuffer.buffer[numChars] = '\0';

                                modified |= ui::property_table::add_input_text(makeId(),
                                    property.name,
                                    stringBuffer.buffer,
                                    sizeof(string_buffer));

                                if (modified)
                                {
                                    property_value_wrapper inputTextWrapper{string_view{stringBuffer.buffer}};
                                    inputTextWrapper.assign_to(property.kind, propertyPtr);
                                }

                                break;
                            }

                            default:
                                ui::property_table::add_empty(property.name);
                                break;
                            }

                            return visit_result::recurse;
                        },
                    });

                ui::property_table::end();
            }

            return modified;
        }

    private:
        bool build_quaternion_editor(const property_node& node, std::byte* const data)
        {
            auto* const q = new (data) quaternion;
            return ui::property_table::add(int(hash_mix(node.offset, 0)), node.name, *q);
        }

        bool build_vec3_editor(const property_node& node, std::byte* const data)
        {
            auto* const v = new (data) vec3;
            return ui::property_table::add(int(hash_mix(node.offset, 0)), node.name, *v);
        }

        bool build_linear_color_editor(const property_node& node, std::byte* const data)
        {
            auto* const v = new (data) vec3;
            return ui::property_table::add_color(int(hash_mix(node.offset, 0)), node.name, *v);
        }

    private:
        struct string_buffer
        {
            char buffer[256];
        };

    private:
        const reflection::reflection_registry* m_reflection{};
        ui::artifact_picker* m_artifactPicker{};
        deque<string_buffer> m_stringBuffers;
    };
}