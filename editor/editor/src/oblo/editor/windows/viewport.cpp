#include <oblo/editor/windows/viewport.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/vulkan/create_render_target.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/destroy_resources.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace oblo::editor
{
    viewport::viewport(vk::vulkan_context& context, ecs::entity_registry& entities) :
        m_ctx{&context}, m_entities{&entities}
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

        OBLO_VK_PANIC(vkCreateSampler(context.get_device(), &samplerInfo, nullptr, &m_sampler));
    }

    viewport::~viewport()
    {
        // We should destroy in the next frame really, since this might be in use, we wait for idle for now
        vkDeviceWaitIdle(m_ctx->get_device());

        vk::reset_texture(*m_ctx, m_texture);

        if (m_descriptorSet)
        {
            ImGui_ImplVulkan_RemoveTexture(m_descriptorSet);
        }

        vk::reset_device_object(m_ctx->get_device(), m_sampler);
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
            if (!m_entity)
            {
                m_entity = m_entities->create<graphics::viewport_component>();
            }

            auto& v = m_entities->get<graphics::viewport_component>(m_entity);
            const auto lastWidth = v.width;
            const auto lastHeight = v.height;

            v.width = u32(windowSize.x);
            v.height = u32(windowSize.y);

            auto& resourceManager = m_ctx->get_resource_manager();
            auto& cb = m_ctx->get_active_command_buffer();

            if (lastWidth != v.width || lastHeight != v.height || !m_texture)
            {
                const auto result = vk::create_2d_render_target(m_ctx->get_allocator(),
                    u32(windowSize.x),
                    u32(windowSize.y),
                    VK_FORMAT_R8G8B8A8_UNORM,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT);

                if (result)
                {
                    if (m_texture)
                    {
                        // auto& oldTexture = resourceManager.get(m_texture);
                        const auto submitIndex = m_ctx->get_submit_index();
                        // m_ctx->destroy_deferred(oldTexture.image, submitIndex);
                        // m_ctx->destroy_deferred(oldTexture.view, submitIndex);
                        // m_ctx->destroy_deferred(oldTexture.allocation, submitIndex);
                        m_ctx->destroy_deferred(m_texture, submitIndex);
                    }

                    if (m_descriptorSet)
                    {
                        // TODO: Sync in a better way
                        vkDeviceWaitIdle(m_ctx->get_device());
                        ImGui_ImplVulkan_RemoveTexture(m_descriptorSet);
                    }

                    m_texture = resourceManager.register_texture(*result, VK_IMAGE_LAYOUT_UNDEFINED);
                    m_descriptorSet =
                        ImGui_ImplVulkan_AddTexture(m_sampler, result->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                    constexpr VkClearColorValue black{};
                    constexpr VkImageSubresourceRange range{
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    };

                    cb.add_pipeline_barrier(resourceManager, m_texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                    vkCmdClearColorImage(cb.get(),
                        result->image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        &black,
                        1,
                        &range);
                }
            }

            v.texture = m_texture;

            if (m_descriptorSet)
            {
                cb.add_pipeline_barrier(resourceManager, m_texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                ImGui::Image(m_descriptorSet, windowSize);
            }

            ImGui::End();
        }

        return open;
    }
}