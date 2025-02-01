#include <oblo/editor/windows/options_editor.hpp>

#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/iterator/handle_range.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/options/options_manager.hpp>

#include <IconsFontAwesome6.h>

#include <imgui.h>

namespace oblo::editor
{
    namespace
    {
        template <typename T>
        bool is_value_within_range(T& v, const void* minPtr, const void* maxPtr)
        {
            return (!minPtr || v >= *reinterpret_cast<const T*>(minPtr)) &&
                (!maxPtr || v <= *reinterpret_cast<const T*>(maxPtr));
        }
    }

    void options_editor::init(const window_update_context& ctx)
    {
        m_options = ctx.services.find<options_manager>();
        OBLO_ASSERT(m_options);
    }

    bool options_editor::update(const window_update_context&)
    {
        bool isOpen{true};

        if (ImGui::Begin("Options", &isOpen))
        {
            string_builder sb;

            auto editorLayer = m_options->get_highest_layer();

            if (ImGui::BeginTable("#table",
                    3,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed);

                for (const auto option : m_options->get_options_range())
                {
                    const auto value = m_options->get_option_value(option);

                    if (value)
                    {
                        const cstring_view name = m_options->get_option_name(option);

                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);

                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(name.begin(), name.end());

                        ImGui::TableSetColumnIndex(1);

                        sb.clear().format("##edit{}", option.value);

                        switch (value->get_kind())
                        {
                        case property_kind::boolean:
                            if (auto v = value->get_bool(); ImGui::Checkbox(sb.c_str(), &v))
                            {
                                m_options->set_option_value(editorLayer, option, property_value_wrapper{v})
                                    .assert_value();
                            }

                            break;

                        case property_kind::u32: {
                            const auto [min, max] = m_options->get_option_value_ranges(option);

                            auto* const minPtr = min.get_kind() == property_kind::u32 ? min.data() : nullptr;
                            auto* const maxPtr = max.get_kind() == property_kind::u32 ? max.data() : nullptr;

                            if (auto v = value->get_u32();
                                ImGui::DragScalar(sb.c_str(), ImGuiDataType_U32, &v, 1.f, minPtr, maxPtr) &&
                                is_value_within_range(v, minPtr, maxPtr))
                            {
                                m_options->set_option_value(editorLayer, option, property_value_wrapper{v})
                                    .assert_value();
                            }
                        }

                        break;

                        case property_kind::f32: {
                            const auto [min, max] = m_options->get_option_value_ranges(option);

                            auto* const minPtr = min.get_kind() == property_kind::f32 ? min.data() : nullptr;
                            auto* const maxPtr = max.get_kind() == property_kind::f32 ? max.data() : nullptr;

                            if (auto v = value->get_f32();
                                ImGui::DragScalar(sb.c_str(), ImGuiDataType_Float, &v, 1.f, minPtr, maxPtr) &&
                                is_value_within_range(v, minPtr, maxPtr))
                            {
                                m_options->set_option_value(editorLayer, option, property_value_wrapper{v})
                                    .assert_value();
                            }
                        }

                        break;

                        default:
                            break;
                        }

                        ImGui::TableSetColumnIndex(2);

                        sb.clear().format(ICON_FA_ARROWS_ROTATE "##reset{}", option.value);

                        if (ImGui::Button(sb.c_str()))
                        {
                            m_options->clear_option_value(editorLayer, option).assert_value();
                        }
                    }
                }

                ImGui::EndTable();
            }
        }

        ImGui::End();

        return isOpen;
    }
}