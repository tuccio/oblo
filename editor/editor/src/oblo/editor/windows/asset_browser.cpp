#include <oblo/editor/windows/asset_browser.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/editor/platform/shell.hpp>

#include <imgui.h>

namespace oblo::editor
{
    asset_browser::asset_browser(asset::asset_registry& registry) :
        m_registry{&registry}, m_path{std::filesystem::canonical(registry.get_asset_directory())}, m_current{m_path}
    {
    }

    bool asset_browser::update()
    {
        bool open{true};

        if (ImGui::Begin("Asset Browser", &open))
        {
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::MenuItem("Import"))
                {
                    std::filesystem::path file;

                    if (platform::open_file_dialog(file))
                    {
                        auto importer = m_registry->create_importer(file);

                        if (importer.is_valid() && importer.init())
                        {
                            importer.execute(m_current);
                        }
                    }
                }

                if (ImGui::MenuItem("Open in Explorer"))
                {
                    platform::open_folder(m_current);
                }

                ImGui::EndPopup();
            }

            if (m_current != m_path)
            {
                if (ImGui::Button(".."))
                {
                    std::error_code ec;
                    m_current = std::filesystem::canonical(m_current / "..", ec);
                    m_breadcrumbs.pop_back();

                    if (ec)
                    {
                        reset_path();
                    }
                }
            }

            std::error_code ec;

            for (auto&& entry : std::filesystem::directory_iterator{m_current, ec})
            {
                const auto& p = entry.path();
                if (std::filesystem::is_directory(p))
                {
                    auto dir = p.filename();
                    const auto& str = dir.u8string();

                    if (ImGui::Button(reinterpret_cast<const char*>(str.c_str())))
                    {
                        m_current = std::filesystem::canonical(p, ec);
                        m_breadcrumbs.emplace_back(std::move(dir));
                    }
                }
                else if (p.native().ends_with(asset::AssetMetaExtension))
                {
                    const auto file = p.filename();
                    const auto& str = file.u8string();
                    ImGui::Button(reinterpret_cast<const char*>(str.c_str()));
                }
            }

            if (ec)
            {
                reset_path();
            }

            ImGui::End();
        }

        return open;
    }

    void asset_browser::reset_path()
    {
        m_current = m_path;
        m_breadcrumbs.clear();
    }
}