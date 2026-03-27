#include <oblo/scene/editor/mesh_inspector.hpp>

#include <oblo/core/data_format.hpp>
#include <oblo/core/iterator/enum_range.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/incremental_id_pool.hpp>
#include <oblo/editor/ui/artifact_picker.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/resources/mesh.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/scene/resources/traits.hpp>

#include <imgui.h>

namespace oblo::editor
{
    namespace
    {
        void format_element(string_builder& builder, const std::byte* data, data_format format, u32 index)
        {
            switch (format)
            {
            case data_format::u8:
                builder.format("{}", reinterpret_cast<const u8*>(data)[index]);
                break;
            case data_format::u16:
                builder.format("{}", reinterpret_cast<const u16*>(data)[index]);
                break;
            case data_format::u32:
                builder.format("{}", reinterpret_cast<const u32*>(data)[index]);
                break;
            case data_format::f32:
                builder.format("{:.3f}", reinterpret_cast<const f32*>(data)[index]);
                break;
            case data_format::vec2: {
                const f32* v = reinterpret_cast<const f32*>(data) + index * 2;
                builder.format("{:.3f}, {:.3f}", v[0], v[1]);
            }
            break;
            case data_format::vec3: {
                const f32* v = reinterpret_cast<const f32*>(data) + index * 3;
                builder.format("{:.3f}, {:.3f}, {:.3f}", v[0], v[1], v[2]);
            }
            break;
            case data_format::vec4: {
                const f32* v = reinterpret_cast<const f32*>(data) + index * 4;
                builder.format("{:.3f}, {:.3f}, {:.3f}, {:.3f}", v[0], v[1], v[2], v[3]);
            }
            break;
            case data_format::vec4u16: {
                const u16* v = reinterpret_cast<const u16*>(data) + index * 4;
                builder.format("{}, {}, {}, {}", v[0], v[1], v[2], v[3]);
            }
            break;
            default:
                builder.append("Unhandled format");
                break;
            }
        }
    }

    class mesh_inspector_window final
    {
    public:
        mesh_inspector_window(const resource_registry& resourceRegistry, uuid resourceId);
        ~mesh_inspector_window();

        bool init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        const resource_registry* m_resourceRegistry{};
        incremental_id_pool* m_idPool{};
        u32 m_id{};
        uuid m_resourceId{};
        resource_ptr<mesh> m_meshPtr{};
        std::unordered_map<hashed_string_view, material_property_descriptor, hash<hashed_string_view>> m_propertyEditor;
        unique_ptr<ui::artifact_picker> m_artifactPicker;
        flags<attribute_kind> m_enabledAttributesMask{};
    };

    mesh_inspector_window::mesh_inspector_window(const resource_registry& resourceRegistry, uuid resourceId) :
        m_resourceRegistry{&resourceRegistry}, m_resourceId{resourceId}
    {
    }

    mesh_inspector_window::~mesh_inspector_window()
    {
        if (m_idPool)
        {
            m_idPool->release<mesh_inspector_window>(m_id);
        }
    }

    bool mesh_inspector_window::init(const window_update_context& ctx)
    {
        m_idPool = ctx.services.find<incremental_id_pool>();

        if (!m_idPool)
        {
            return false;
        }

        m_id = m_idPool->acquire<mesh_inspector_window>();

        const resource_ptr<mesh> meshPtr = m_resourceRegistry->get_resource(m_resourceId).as<mesh>();

        if (!meshPtr)
        {
            return false;
        }

        m_meshPtr = meshPtr;
        m_meshPtr.load_start_async();

        m_enabledAttributesMask = flags<attribute_kind>::all();

        return true;
    }

    bool mesh_inspector_window::update(const window_update_context&)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

        bool isOpen{true};

        string_builder builder;
        builder.format("Mesh Inspector##{}", m_id);

        if (ImGui::Begin(builder.c_str(), &isOpen))
        {
            if (!m_meshPtr || m_meshPtr.is_invalidated())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_NavHighlight));
                ImGui::TextUnformatted("The mesh was invalidated, please reload to load the new resource");
                ImGui::PopStyleColor();

                if (ImGui::Button("Reload"))
                {
                    m_meshPtr = m_resourceRegistry->get_resource(m_resourceId).as<mesh>();
                    m_meshPtr.load_start_async();
                }
            }

            if (m_meshPtr.is_successfully_loaded())
            {
                const mesh& mesh = *m_meshPtr;

                if (ImGui::CollapsingHeader("Topology", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    constexpr const attribute_kind kinds[] = {
                        attribute_kind::indices,
                        attribute_kind::microindices,
                    };

                    constexpr const char* labels[] = {"Indices", "Microindices"};

                    for (u32 i = 0; i < 2; ++i)
                    {
                        const auto kind = kinds[i];

                        if (!mesh.has_attribute(kind))
                        {
                            continue;
                        }

                        if (ImGui::TreeNode(labels[i]))
                        {
                            const auto bytes = mesh.get_attribute(kind);
                            const auto format = mesh.get_attribute_format(kind);
                            const u32 count = mesh.get_elements_count(kind);

                            if (ImGui::BeginTable("topology_table",
                                    2,
                                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                                    ImVec2(0, 150)))
                            {
                                ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                                ImGui::TableHeadersRow();

                                ImGuiListClipper clipper;
                                clipper.Begin(count);

                                while (clipper.Step())
                                {
                                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                                    {
                                        ImGui::TableNextRow();
                                        ImGui::TableSetColumnIndex(0);
                                        ImGui::TextDisabled("[%d]", row);

                                        ImGui::TableSetColumnIndex(1);
                                        builder.clear();

                                        format_element(builder, bytes.data(), format, row);
                                        ImGui::TextUnformatted(builder.c_str());
                                    }
                                }

                                ImGui::EndTable();
                            }

                            ImGui::TreePop();
                        }
                    }
                }

                if (ImGui::CollapsingHeader("Vertex Attributes", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    constexpr const char* labels[] = {
                        "Indices",
                        "Microindices",
                        "Position",
                        "Normal",
                        "Tangent",
                        "Bitangent",
                        "UV0",
                        "Joint Indices",
                        "Joint Weights",
                    };

                    ImGui::TextUnformatted("Columns:");
                    ImGui::SameLine();

                    u32 activeColumns = 1;

                    for (const attribute_kind kind : enum_range<attribute_kind>{})
                    {
                        if (is_vertex_attribute(kind) && mesh.has_attribute(kind))
                        {
                            bool isEnabled = m_enabledAttributesMask.contains(kind);

                            if (ImGui::Checkbox(labels[u8(kind)], &isEnabled))
                            {
                                m_enabledAttributesMask.assign(kind, isEnabled);
                            }

                            if (isEnabled)
                            {
                                ++activeColumns;
                            }

                            ImGui::SameLine();
                        }
                    }

                    ImGui::NewLine();

                    if (ImGui::BeginTable("vertex_table",
                            activeColumns,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                ImGuiTableFlags_Resizable))
                    {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("Vertex", ImGuiTableColumnFlags_WidthFixed, 45.0f);

                        for (const attribute_kind kind : enum_range<attribute_kind>{})
                        {
                            if (is_vertex_attribute(kind) && mesh.has_attribute(kind) &&
                                m_enabledAttributesMask.contains(kind))
                            {
                                ImGui::TableSetupColumn(labels[u8(kind)]);
                            }
                        }

                        ImGui::TableHeadersRow();

                        const u32 vertex_count = mesh.get_vertex_count();
                        ImGuiListClipper clipper;
                        clipper.Begin(vertex_count);

                        while (clipper.Step())
                        {
                            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::TextDisabled("%d", row);

                                u32 current_col = 1;
                                for (const attribute_kind kind : enum_range<attribute_kind>{})
                                {
                                    if (is_vertex_attribute(kind) && mesh.has_attribute(kind) &&
                                        m_enabledAttributesMask.contains(kind))
                                    {
                                        ImGui::TableSetColumnIndex(current_col++);

                                        const auto bytes = mesh.get_attribute(kind);
                                        const auto format = mesh.get_attribute_format(kind);

                                        builder.clear();
                                        format_element(builder, bytes.data(), format, row);
                                        ImGui::TextUnformatted(builder.c_str());
                                    }
                                }
                            }
                        }

                        ImGui::EndTable();
                    }
                }
            }
        }

        ImGui::End();

        ImGui::PopStyleVar(2);

        return isOpen;
    }

    expected<> mesh_inspector::open(
        window_manager& wm, const resource_registry& resourceRegistry, window_handle parent, uuid resourceId)
    {
        const auto h = wm.create_child_window<mesh_inspector_window>(parent, {}, {}, resourceRegistry, resourceId);

        if (!h)
        {
            return "Editor operation failed"_err;
        }

        m_editor = h;

        return no_error;
    }

    void mesh_inspector::close(window_manager& wm)
    {
        wm.destroy_window(m_editor);
        m_editor = {};
    }

    window_handle mesh_inspector::get_window() const
    {
        return m_editor;
    }
}