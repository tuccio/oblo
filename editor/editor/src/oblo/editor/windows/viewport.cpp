#include <oblo/editor/windows/viewport.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/vulkan/create_render_target.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace oblo::editor
{
    viewport::viewport(const vk::allocator& allocator,
                       const vk::single_queue_engine& engine,
                       vk::resource_manager& resourceManager,
                       ecs::entity_registry& entities) :
        m_allocator{&allocator},
        m_resourceManager{&resourceManager}, m_entities{&entities}
    {
        constexpr VkSamplerCreateInfo samplerInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .anisotropyEnable = VK_FALSE,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        };

        OBLO_VK_PANIC(vkCreateSampler(engine.get_device(), &samplerInfo, nullptr, &m_sampler));
    }

    viewport::~viewport()
    {
        // TODO: Delete texture (or better just use a pool)
        // TODO: Delete the imgui texture (or just handle the descriptor ourselves)
    }

    bool viewport::update()
    {
        bool open{true};

        if (ImGui::Begin("Viewport", &open))
        {
            const auto regionMin = ImGui::GetWindowContentRegionMin();
            const auto regionMax = ImGui::GetWindowContentRegionMax();

            const auto windowSize = ImVec2{regionMax.x - regionMin.x, regionMax.y - regionMin.y};

            // TODO: handle resize
            if (!m_texture)
            {
                const auto result = vk::create_2d_render_target(*m_allocator,
                                                                u32(windowSize.x),
                                                                u32(windowSize.y),
                                                                VK_FORMAT_R8G8B8A8_UNORM,
                                                                VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                VK_IMAGE_ASPECT_COLOR_BIT);

                if (result)
                {
                    m_texture = m_resourceManager->register_texture(*result, VK_IMAGE_LAYOUT_UNDEFINED);
                    m_imguiImage =
                        ImGui_ImplVulkan_AddTexture(m_sampler, result->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }

                const auto e = m_entities->create<graphics::viewport_component>();
                m_entities->get<graphics::viewport_component>(e).texture = m_texture;
            }

            if (m_imguiImage)
            {
                ImGui::Image(m_imguiImage, windowSize);
            }

            ImGui::End();
        }

        return open;
    }
}