#include "dotnet_bindings.hpp"

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/math/forward.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/reflection/reflection_registry.hpp>

namespace oblo::reflection
{
    struct script_api;
}

namespace oblo::gen::dotnet
{
    namespace
    {
        constexpr u32 g_ManagedClassIndent = 1;

        constexpr string_view to_csharp_type(property_kind kind)
        {
            switch (kind)
            {
            case property_kind::boolean:
                return "bool";
            case property_kind::u8:
                return "byte";
            case property_kind::u16:
                return "ushort";
            case property_kind::u32:
                return "uint";
            case property_kind::u64:
                return "ulong";
            case property_kind::i8:
                return "sbyte";
            case property_kind::i16:
                return "short";
            case property_kind::i32:
                return "int";
            case property_kind::i64:
                return "long";
            case property_kind::f32:
                return "float";
            case property_kind::f64:
                return "double";
            case property_kind::uuid:
                return "Guid";
            case property_kind::string:
                return "string";
            default:
                unreachable();
            }
        }

        void to_pascal_case(string_view snakeOrCamelCase, string_builder& outPascalCase)
        {
            bool capitalizeNext = true;

            for (char c : snakeOrCamelCase)
            {
                if (c == '_')
                {
                    capitalizeNext = true;
                }
                else
                {
                    if (capitalizeNext)
                    {
                        outPascalCase.append(static_cast<char>(std::toupper(c)));
                        capitalizeNext = false;
                    }
                    else
                    {
                        outPascalCase.append(c);
                    }
                }
            }
        }

        string_view extract_class_name(string_view typeName)
        {
            // struct n1::n2::my_class<n3::xdd>

            const auto firstTemplate = typeName.find_first_of('<');

            usize begin = 0;

            while (true)
            {
                const auto firstNameSpace = typeName.find_first_of(':', begin);

                if (firstNameSpace == string_view::npos || firstNameSpace > firstTemplate)
                {
                    break;
                }

                begin = firstNameSpace + 1;
            }

            return typeName.substr(begin, firstTemplate - begin);
        }

        void indent(string_builder& b, u32 n)
        {
            for (u32 i = 0; i < n; ++i)
            {
                b.append("    ");
            }
        }

        void begin_component(string_builder& nativeCode, string_builder& managedCode, string_view className)
        {
            indent(managedCode, g_ManagedClassIndent);
            managedCode.append("public struct ");
            to_pascal_case(className, managedCode);
            managedCode.append('\n');

            indent(managedCode, g_ManagedClassIndent);
            managedCode.append("{\n");

            indent(managedCode, g_ManagedClassIndent + 1);
            managedCode.append("public Entity Entity => _entity;\n");

            indent(managedCode, g_ManagedClassIndent + 1);
            managedCode.append("public ComponentTypeId TypeId => _typeId;\n");

            indent(managedCode, g_ManagedClassIndent + 1);
            managedCode.append("private readonly Entity _entity;\n");

            indent(managedCode, g_ManagedClassIndent + 1);
            managedCode.append("private readonly ComponentTypeId _typeId;\n");

            indent(managedCode, g_ManagedClassIndent + 1);
            managedCode.append("public ");
            to_pascal_case(className, managedCode);
            managedCode.append("(Entity entity, ComponentTypeId typeId) { _entity = entity; _typeId = typeId; }\n");

            nativeCode.append("// Begin component ");
            nativeCode.append(className);
            nativeCode.append('\n');
        }

        void end_component(string_builder& nativeCode, string_builder& managedCode, string_view className)
        {
            indent(managedCode, g_ManagedClassIndent);
            managedCode.append("}\n");

            nativeCode.append("// End component ");
            nativeCode.append(className);
            nativeCode.append('\n');
        }

        void add_component_property(string_builder& managedCode, const property& p)
        {
            if (p.kind != property_kind::string)
            {
                const auto csharpType = to_csharp_type(p.kind);

                indent(managedCode, g_ManagedClassIndent + 1);
                managedCode.append("public ");
                managedCode.append(csharpType);
                managedCode.append(" ");
                to_pascal_case(p.name, managedCode);
                managedCode.append('\n');

                indent(managedCode, g_ManagedClassIndent + 1);
                managedCode.append("{\n");

                indent(managedCode, g_ManagedClassIndent + 2);
                managedCode.format("get {{ Ecs.Bindings.GetComponentPropertyRaw(Entity, TypeId, {0}, out {1} result); "
                                   "return result; }}\n",
                    p.offset,
                    csharpType);

                indent(managedCode, g_ManagedClassIndent + 2);
                managedCode.format("set {{ Ecs.Bindings.SetComponentPropertyRaw(Entity, TypeId, {0}, ref value); }}\n",
                    p.offset);

                indent(managedCode, g_ManagedClassIndent + 1);
                managedCode.append("}\n\n");
            }
        }
    }

    expected<> generate_bindings(const reflection::reflection_registry& reflectionRegistry,
        const ecs::type_registry& typeRegistry,
        const property_registry& propertyRegistry,
        cstring_view nativePath,
        cstring_view managedPath)
    {
        string_builder nativeCode;
        string_builder managedCode;

        managedCode.append("namespace Oblo\n{\n");

        for (auto& component : typeRegistry.get_component_types())
        {
            const auto reflectionId = reflectionRegistry.find_type(component.type);

            const auto* propertyTree = propertyRegistry.try_get(component.type);

            if (!reflectionId || !propertyTree || propertyTree->nodes.empty())
            {
                continue;
            }

            if (!reflectionRegistry.has_tag<reflection::script_api>(reflectionId))
            {
                continue;
            }

            const auto className = extract_class_name(component.type.name);

            begin_component(nativeCode, managedCode, className);

            auto& root = propertyTree->nodes[0];

            for (u32 propertyIndex = root.firstProperty; propertyIndex != root.lastProperty; ++propertyIndex)
            {
                const auto& property = propertyTree->properties[propertyIndex];

                add_component_property(managedCode, property);
            }

            for (u32 nodeIndex = root.firstChild; nodeIndex != 0;
                nodeIndex = propertyTree->nodes[nodeIndex].firstSibling)
            {
                auto& node = propertyTree->nodes[nodeIndex];

                if (node.type == get_type_id<vec3>())
                {
                }

                // Add nodes
            }

            end_component(nativeCode, managedCode, component.type.name);
        }

        managedCode.append("}"); // namespace Oblo

        const auto native = !filesystem::write_file(nativePath, as_bytes(std::span{nativeCode}), {});
        const auto managed = !filesystem::write_file(managedPath, as_bytes(std::span{managedCode}), {});

        if (!native || !managed)
        {
            return unspecified_error;
        }

        return no_error;
    }
}