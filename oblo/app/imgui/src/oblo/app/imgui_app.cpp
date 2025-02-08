#include <oblo/app/imgui_app.hpp>

#include <oblo/app/graphics_engine.hpp>
#include <oblo/app/graphics_window.hpp>
#include <oblo/app/graphics_window_context.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/core/allocation_helpers.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/graph/frame_graph_registry.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/templates/graph_templates.hpp>
#include <oblo/vulkan/utility.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

#include <imgui_impl_sdl2.h>

namespace oblo
{
    namespace
    {
        constexpr const char* g_WindowGraphicsContext = "oblo::gfx";

        void imgui_sdl_dispatch(const void* event)
        {
            ImGui_ImplSDL2_ProcessEvent(static_cast<const SDL_Event*>(event));
        }

        SDL_Window* get_sdl_window(const graphics_window& window);
        graphics_window_context* get_graphics_context(const graphics_window& window);

        template <auto Impl, auto Context>
        struct graphics_window_accessor
        {
            friend SDL_Window* get_sdl_window(const graphics_window& window)
            {
                return static_cast<SDL_Window*>(window.*Impl);
            }

            friend graphics_window_context* get_graphics_context(const graphics_window& window)
            {
                return window.*Context;
            }
        };

        template struct graphics_window_accessor<&graphics_window::m_impl, &graphics_window::m_graphicsContext>;

        struct imgui_render_userdata
        {
            graphics_window_context* windowContext{};
            h32<vk::frame_graph_subgraph> graph;
            bool isOwned;
        };

        struct imgui_render_backend
        {
            graphics_engine* graphicsEngine{};
            vk::renderer* renderer{};
            vk::frame_graph_registry nodeRegistry;
            vk::frame_graph_template renderTemplate;

            void* create_render_userdata() {}
        };

        struct render_imgui_node
        {
            vk::data<ImGuiViewport*> inViewport;

            vk::resource<vk::texture> inOutRenderTarget;

            vk::resource<vk::buffer> outVertexBuffer;
            vk::resource<vk::buffer> outIndexBuffer;

            h32<vk::render_pass> renderPass;
            h32<vk::render_pass_instance> renderPassInstance;

            void init(const vk::frame_graph_init_context& ctx)
            {
                renderPass = ctx.register_render_pass({
                    .name = "ImGui",
                    .stages =
                        {
                            {
                                .stage = vk::pipeline_stages::vertex,
                                .shaderSourcePath = "./imgui/shaders/imgui.vert",
                            },
                            {
                                .stage = vk::pipeline_stages::fragment,
                                .shaderSourcePath = "./imgui/shaders/imgui.frag",
                            },
                        },
                });
            }

            void build(const vk::frame_graph_build_context& ctx)
            {
                if (!ctx.has_source(inOutRenderTarget))
                {
                    renderPassInstance = {};
                    return;
                }

                ImGuiViewport* const viewport = ctx.access(inViewport);
                OBLO_ASSERT(viewport);

                auto* const drawData = viewport->DrawData;

                const auto rtInitializer =
                    ctx.get_current_initializer(inOutRenderTarget).value_or(vk::image_initializer{});

                renderPassInstance = ctx.render_pass(renderPass,
                    {
                        .renderTargets =
                            {
                                .colorAttachmentFormats = {rtInitializer.format},
                            },
                        .depthStencilState =
                            {
                                .depthTestEnable = false,
                                .depthWriteEnable = false,
                            },
                        .rasterizationState =
                            {
                                .polygonMode = VK_POLYGON_MODE_FILL,
                                .cullMode = VK_CULL_MODE_NONE,
                                .lineWidth = 1.f,
                            },
                    });

                ctx.acquire(inOutRenderTarget, vk::texture_usage::render_target_write);

                const std::span vertices =
                    allocate_n_span<ImDrawVert>(ctx.get_frame_allocator(), usize(drawData->TotalVtxCount));

                const std::span indices =
                    allocate_n_span<ImDrawIdx>(ctx.get_frame_allocator(), usize(drawData->TotalIdxCount));

                auto vertexIt = vertices.data();
                auto indexIt = indices.data();

                for (int n = 0; n < drawData->CmdListsCount; ++n)
                {
                    const ImDrawList* drawList = drawData->CmdLists[n];

                    std::memcpy(vertexIt, drawList->VtxBuffer.Data, sizeof(ImDrawVert) * drawList->VtxBuffer.Size);
                    std::memcpy(indexIt, drawList->IdxBuffer.Data, sizeof(ImDrawIdx) * drawList->IdxBuffer.Size);

                    vertexIt += drawList->VtxBuffer.Size;
                    indexIt += drawList->IdxBuffer.Size;
                }

                ctx.create(outVertexBuffer,
                    vk::buffer_resource_initializer{
                        .size = u32(vertices.size_bytes()),
                        .data = as_bytes(vertices),
                    },
                    vk::buffer_usage::storage_read);

                ctx.create(outIndexBuffer,
                    vk::buffer_resource_initializer{
                        .size = u32(indices.size_bytes()),
                        .data = as_bytes(indices),
                    },
                    vk::buffer_usage::index);
            }

            void execute(const vk::frame_graph_execute_context& ctx)
            {
                using namespace vk;

                if (!renderPassInstance)
                {
                    return;
                }

                const auto renderTarget = ctx.access(inOutRenderTarget);

                const VkRenderingAttachmentInfo colorAttachments[] = {
                    {
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .imageView = renderTarget.view,
                        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .clearValue =
                            VkClearValue{
                                .color = {0, 0, 0, 0},
                            },
                    },
                };

                const auto [renderWidth, renderHeight, _] = renderTarget.initializer.extent;

                const VkRenderingInfo renderInfo{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .renderArea =
                        {
                            .extent{
                                .width = renderWidth,
                                .height = renderHeight,
                            },
                        },
                    .layerCount = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = colorAttachments,
                };

                if (ctx.begin_pass(renderPassInstance, renderInfo))
                {
                    const VkCommandBuffer commandBuffer = ctx.get_command_buffer();

                    vk::setup_viewport_scissor(commandBuffer, renderWidth, renderHeight);

                    struct push_constants
                    {
                        vec2 scale;
                        vec2 translate;
                    };

                    binding_table bindingTable;

                    bindingTable.bind_buffers({
                        {"b_VertexData"_hsv, outVertexBuffer},
                    });

                    ctx.bind_descriptor_sets(bindingTable);

                    auto indexBuffer = ctx.access(outIndexBuffer);

                    vkCmdBindIndexBuffer(commandBuffer,
                        indexBuffer.buffer,
                        indexBuffer.offset,
                        sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

                    ImGuiViewport* const viewport = ctx.access(inViewport);
                    OBLO_ASSERT(viewport);
                    auto* const drawData = viewport->DrawData;

                    push_constants pushConstants{};
                    pushConstants.scale = vec2::splat(2.f) / vec2{drawData->DisplaySize.x, drawData->DisplaySize.y};
                    pushConstants.translate =
                        vec2::splat(-1.f) - pushConstants.scale * vec2{drawData->DisplayPos.x, drawData->DisplayPos.y};

                    ctx.push_constants(shader_stage::vertex, 0, as_bytes(std::span{&pushConstants, 1}));

                    // Will project scissor/clipping rectangles into framebuffer space
                    ImVec2 clip_off = drawData->DisplayPos; // (0,0) unless using multi-viewports
                    ImVec2 clip_scale =
                        drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

                    // Render command lists
                    // (Because we merged all buffers into a single one, we maintain our own offset into them)
                    int global_vtx_offset = 0;
                    int global_idx_offset = 0;
                    for (const ImDrawList* draw_list : drawData->CmdLists)
                    {
                        for (const ImDrawCmd& pcmd : draw_list->CmdBuffer)
                        {
                            // Project scissor/clipping rectangles into framebuffer space
                            vec2 clip_min{
                                (pcmd.ClipRect.x - clip_off.x) * clip_scale.x,
                                (pcmd.ClipRect.y - clip_off.y) * clip_scale.y,
                            };

                            vec2 clip_max{
                                (pcmd.ClipRect.z - clip_off.x) * clip_scale.x,
                                (pcmd.ClipRect.w - clip_off.y) * clip_scale.y,
                            };

                            // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                            if (clip_min.x < 0.0f)
                            {
                                clip_min.x = 0.0f;
                            }

                            if (clip_min.y < 0.0f)
                            {
                                clip_min.y = 0.0f;
                            }

                            if (clip_max.x > renderWidth)
                            {
                                clip_max.x = (f32) renderWidth;
                            }

                            if (clip_max.y > renderHeight)
                            {
                                clip_max.y = (f32) renderHeight;
                            }

                            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                            {
                                continue;
                            }

                            // Apply scissor/clipping rectangle
                            const VkRect2D scissor{
                                .offset =
                                    {
                                        .x = (i32) (clip_min.x),
                                        .y = (i32) (clip_min.y),
                                    },
                                .extent =
                                    {
                                        .width = (u32) (clip_max.x - clip_min.x),
                                        .height = (u32) (clip_max.y - clip_min.y),
                                    },
                            };

                            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                            vkCmdDrawIndexed(commandBuffer,
                                pcmd.ElemCount,
                                1,
                                pcmd.IdxOffset + global_idx_offset,
                                pcmd.VtxOffset + global_vtx_offset,
                                0);
                        }

                        global_idx_offset += draw_list->IdxBuffer.Size;
                        global_vtx_offset += draw_list->VtxBuffer.Size;
                    }

                    ctx.end_pass();
                }
            }
        };

        constexpr string_view g_InImGuiViewport{"ImGuiViewport"};
        constexpr string_view g_InOutRT{"RenderTarget"};

        vk::frame_graph_template create_imgui_frame_graph_template(vk::frame_graph_registry& registry)
        {
            registry.register_node<render_imgui_node>();

            vk::frame_graph_template fgt;
            fgt.init(registry);

            auto render = fgt.add_node<render_imgui_node>();
            fgt.make_input(render, &render_imgui_node::inViewport, g_InImGuiViewport);
            fgt.make_input(render, &render_imgui_node::inOutRenderTarget, g_InOutRT);
            fgt.make_output(render, &render_imgui_node::inOutRenderTarget, g_InOutRT);

            return fgt;
        }

        imgui_render_userdata* create_render_userdata(const imgui_render_backend* backend,
            graphics_window_context* windowContext,
            bool isOwned,
            ImGuiViewport* viewport)
        {
            auto& frameGraph = backend->renderer->get_frame_graph();

            auto graph = frameGraph.instantiate(backend->renderTemplate);
            frameGraph.set_input(graph, g_InImGuiViewport, viewport).assert_value();

            return IM_NEW(imgui_render_userdata){
                .windowContext = windowContext,
                .graph = graph,
                .isOwned = isOwned,
            };
        }

        void destroy_render_userdata(const imgui_render_backend* backend, imgui_render_userdata* rud)
        {
            if (rud->isOwned && rud->windowContext)
            {
                rud->windowContext->on_destroy();
            }

            auto& frameGraph = backend->renderer->get_frame_graph();
            frameGraph.remove(rud->graph);

            IM_DELETE(rud);
        }

        void connect_viewport(vk::frame_graph& frameGraph, ImGuiViewport* viewport)
        {
            auto* const rud = static_cast<imgui_render_userdata*>(viewport->RendererUserData);
            const auto swapchainGraph = rud->windowContext->get_swapchain_graph();

            if (swapchainGraph)
            {
                frameGraph.connect(swapchainGraph, vk::swapchain_graph::OutAcquiredImage, rud->graph, g_InOutRT);
                frameGraph.connect(rud->graph, g_InOutRT, swapchainGraph, vk::swapchain_graph::InRenderedImage);
            }
        }
    }

    struct imgui_app::impl
    {
        ImGuiContext* context{};
        imgui_render_backend backend{};

        bool init(const graphics_window& window)
        {
            if (!window.is_ready())
            {
                return false;
            }

            context = ImGui::CreateContext();

            auto& io = ImGui::GetIO();

            io.ConfigFlags |=
                ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable | ImGuiConfigFlags_NavEnableKeyboard;

            auto* const sdlWindow = get_sdl_window(window);

            if (!ImGui_ImplSDL2_InitForOther(sdlWindow))
            {
                return false;
            }

            return init_render_backend(window);
        }

        bool init_render_backend(const graphics_window& window)
        {
            auto& mm = module_manager::get();

            auto* const gfxEngine = mm.find_unique_service<graphics_engine>();

            if (!gfxEngine)
            {
                return false;
            }

            auto* const vkEngine = mm.find<vk::vulkan_engine_module>();

            if (!vkEngine)
            {
                return false;
            }

            // TODO: Create the frame graph for the main window
            ImGuiIO& io = ImGui::GetIO();
            OBLO_ASSERT(io.BackendRendererUserData == nullptr);

            backend.graphicsEngine = gfxEngine;
            backend.renderer = &vkEngine->get_renderer();
            backend.renderTemplate = create_imgui_frame_graph_template(backend.nodeRegistry);

            io.BackendRendererUserData = &backend;
            io.BackendRendererName = "oblo";
            io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset
                                                                       // field, allowing for large meshes.
            io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the
                                                                       // Renderer side (optional)

            ImGuiViewport* viewport = ImGui::GetMainViewport();
            viewport->RendererUserData =
                create_render_userdata(&backend, get_graphics_context(window), false, viewport);

            ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();

            platformIO.Renderer_CreateWindow = [](ImGuiViewport* viewport)
            {
                auto* backend = static_cast<imgui_render_backend*>(ImGui::GetIO().BackendRendererUserData);

                auto* graphicsContext = backend->graphicsEngine->create_context(viewport->PlatformHandleRaw,
                    u32(viewport->Size.x),
                    u32(viewport->Size.y));

                viewport->RendererUserData = create_render_userdata(backend, graphicsContext, true, viewport);
            };

            platformIO.Renderer_DestroyWindow = [](ImGuiViewport* viewport)
            {
                auto* backend = static_cast<imgui_render_backend*>(ImGui::GetIO().BackendRendererUserData);
                auto* rud = static_cast<imgui_render_userdata*>(viewport->RendererUserData);
                destroy_render_userdata(backend, rud);

                viewport->RendererUserData = nullptr;
            };

            platformIO.Renderer_SetWindowSize = [](ImGuiViewport* viewport, ImVec2 size)
            {
                auto* rud = static_cast<imgui_render_userdata*>(viewport->RendererUserData);
                rud->windowContext->on_resize(u32(size.x), u32(size.y));
            };

            // TODO: Create font texture
            unsigned char* pixels;
            int width, height;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            // size_t upload_size = width * height * 4 * sizeof(char);

            return true;
        }

        ~impl()
        {
            if (context)
            {
                ImGui_ImplSDL2_Shutdown();

                ImGui::DestroyContext(context);
                context = nullptr;
            }
        }
    };

    imgui_app::imgui_app() = default;

    imgui_app::~imgui_app() = default;

    bool imgui_app::init(const graphics_window& window)
    {
        if (m_impl)
        {
            return false;
        }

        m_impl = allocate_unique<impl>();
        return m_impl->init(window);
    }

    void imgui_app::shutdown()
    {
        m_impl.reset();
    }

    void imgui_app::begin_frame()
    {
        OBLO_PROFILE_SCOPE();

        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void imgui_app::render()
    {
        OBLO_PROFILE_SCOPE();
        ImGui::Render();

        // Update and Render additional Platform Windows
        auto& io = ImGui::GetIO();

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
        }

        auto& fg = m_impl->backend.renderer->get_frame_graph();

        auto& platformIO = ImGui::GetPlatformIO();

        for (auto* viewport : platformIO.Viewports)
        {
            if (viewport->RendererUserData)
            {
                connect_viewport(fg, viewport);
            }
        }
    }

    void imgui_app::end_frame()
    {
        OBLO_PROFILE_SCOPE();
    }

    window_event_dispatcher imgui_app::get_event_dispatcher()
    {
        return window_event_dispatcher{.dispatch = imgui_sdl_dispatch};
    }
}