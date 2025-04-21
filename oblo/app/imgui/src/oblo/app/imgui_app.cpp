#include <oblo/app/imgui_app.hpp>

#include <oblo/app/graphics_engine.hpp>
#include <oblo/app/graphics_window.hpp>
#include <oblo/app/graphics_window_context.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/resources/texture.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/draw/types.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/graph/frame_graph_registry.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/graph/render_pass.hpp>
#include <oblo/vulkan/templates/graph_templates.hpp>
#include <oblo/vulkan/utility.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

#include <imgui_impl_win32.h>

#include <Windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace oblo
{
    namespace
    {
        void imgui_win32_dispatch_event(const void* event)
        {
            const MSG* msg = reinterpret_cast<const MSG*>(event);
            ImGui_ImplWin32_WndProcHandler(msg->hwnd, msg->message, msg->wParam, msg->lParam);
        }

        void* get_win32_window(const graphics_window& window);
        graphics_window_context* get_graphics_context(const graphics_window& window);

        template <auto Impl, auto Context>
        struct graphics_window_accessor
        {
            friend void* get_win32_window(const graphics_window& window)
            {
                return static_cast<void*>(window.*Impl);
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
            const resource_registry* resourceRegistry{};
            graphics_engine* graphicsEngine{};
            vk::frame_graph* frameGraph{};
            vk::frame_graph_registry nodeRegistry;
            vk::frame_graph_template renderTemplate;
            vk::frame_graph_template pushImageTemplate;
            deque<h32<vk::frame_graph_subgraph>> pushImageSubgraphs;

            std::unordered_map<resource_ref<texture>, usize, hash<resource_ref<texture>>> registeredTexturesMap;
            deque<resource_ptr<texture>> registeredTextures;

            ImTextureID nextTextureId{};

            static constexpr u64 subgraph_image_bit = u64{1} << 63;

            ImTextureID register_texture(resource_ref<texture> ref)
            {
                const auto [it, inserted] = registeredTexturesMap.emplace(ref, nextTextureId);

                if (inserted)
                {
                    resource_ptr ptr = resourceRegistry->get_resource(ref);
                    OBLO_ASSERT(ptr);

                    if (ptr)
                    {
                        ptr.load_start_async();
                    }

                    return register_texture(std::move(ptr));
                }

                return it->second;
            }

            ImTextureID register_texture(resource_ptr<texture> ptr)
            {
                const auto newId = nextTextureId;

                registeredTextures.resize(newId + 1);

                registeredTextures[newId] = std::move(ptr);
                ++nextTextureId;

                return newId;
            }

            ImTextureID register_subgraph_image(h32<vk::frame_graph_subgraph> subgraph)
            {
                const auto newId = ImTextureID{subgraph_image_bit | pushImageSubgraphs.size()};
                pushImageSubgraphs.emplace_back(subgraph);
                return newId;
            }

            static bool is_subgraph_texture(ImTextureID id)
            {
                return (subgraph_image_bit & id) != 0;
            }

            static u64 extract_subgraph_texture_index(ImTextureID id)
            {
                OBLO_ASSERT(is_subgraph_texture(id));
                return ~subgraph_image_bit & id;
            }
        };

        struct imgui_graph_image
        {
            vk::resource<vk::texture> texture;
            ImTextureID id;
        };

        struct imgui_graph_image_push_node
        {
            vk::data<ImTextureID> inId;
            vk::resource<vk::texture> inTexture;
            vk::data_sink<imgui_graph_image> outImageSink;

            void build(const vk::frame_graph_build_context& ctx)
            {
                if (ctx.has_source(inTexture))
                {
                    const ImTextureID id = ctx.access(inId);
                    ctx.push(outImageSink, {inTexture, id});
                }
            }
        };

        struct imgui_render_node
        {
            vk::data<const imgui_render_backend*> inBackend;
            vk::data<ImGuiViewport*> inViewport;

            vk::resource<vk::texture> inOutRenderTarget;

            vk::data_sink<imgui_graph_image> inImageSink;

            vk::resource<vk::buffer> outVertexBuffer;
            vk::resource<vk::buffer> outIndexBuffer;

            h32<vk::render_pass> renderPass;
            h32<vk::render_pass_instance> renderPassInstance;

            std::span<h32<vk::resident_texture>> registeredTextures;
            std::span<h32<vk::resident_texture>> subgraphTextures;

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

                const auto rtInitializer =
                    ctx.get_current_initializer(inOutRenderTarget).value_or(vk::texture_init_desc{});

                renderPassInstance = ctx.render_pass(renderPass,
                    {
                        .renderTargets =
                            {
                                .colorAttachmentFormats = {vk::texture_format(rtInitializer.format)},
                                .blendStates =
                                    {
                                        {
                                            .enable = true,
                                            .srcColorBlendFactor = vk::blend_factor::src_alpha,
                                            .dstColorBlendFactor = vk::blend_factor::one_minus_src_alpha,
                                            .colorBlendOp = vk::blend_op::add,
                                            .srcAlphaBlendFactor = vk::blend_factor::one,
                                            .dstAlphaBlendFactor = vk::blend_factor::one_minus_src_alpha,
                                            .alphaBlendOp = vk::blend_op::add,
                                            .colorWriteMask = vk::color_component::r | vk::color_component::g |
                                                vk::color_component::b | vk::color_component::a,
                                        },
                                    },
                            },
                        .depthStencilState =
                            {
                                .depthTestEnable = false,
                                .depthWriteEnable = false,
                            },
                        .rasterizationState =
                            {
                                .polygonMode = vk::polygon_mode::fill,
                                .cullMode = {},
                                .lineWidth = 1.f,
                            },
                    });

                auto* const backend = ctx.access(inBackend);
                OBLO_ASSERT(backend);

                registeredTextures = allocate_n_span<h32<vk::resident_texture>>(ctx.get_frame_allocator(),
                    backend->registeredTextures.size());

                subgraphTextures = allocate_n_span<h32<vk::resident_texture>>(ctx.get_frame_allocator(),
                    backend->pushImageSubgraphs.size());

                for (usize i = 0; i < backend->registeredTextures.size(); ++i)
                {
                    registeredTextures[i] = ctx.load_resource(backend->registeredTextures[i]);
                }

                for (auto& graphImage : ctx.access(inImageSink))
                {
                    const auto textureId = backend->extract_subgraph_texture_index(graphImage.id);

                    subgraphTextures[textureId] =
                        ctx.acquire_bindless(graphImage.texture, vk::texture_usage::shader_read);
                }

                ImGuiViewport* const viewport = ctx.access(inViewport);
                OBLO_ASSERT(viewport);

                auto* const drawData = viewport->DrawData;

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

                const render_attachment colorAttachments[] = {
                    {
                        .texture = inOutRenderTarget,
                        .loadOp = attachment_load_op::clear,
                        .storeOp = attachment_store_op::store,
                    },
                };

                const render_pass_config cfg{
                    .renderResolution = ctx.get_resolution(inOutRenderTarget),
                    .colorAttachments = colorAttachments,
                };

                if (ctx.begin_pass(renderPassInstance, cfg))
                {
                    ctx.set_viewport(cfg.renderResolution.x, cfg.renderResolution.y);

                    struct transform_constants
                    {
                        vec2 scale;
                        vec2 translation;
                    };

                    binding_table bindingTable;

                    bindingTable.bind_buffers({
                        {"b_VertexData"_hsv, outVertexBuffer},
                    });

                    ctx.bind_descriptor_sets(bindingTable);

                    ctx.bind_index_buffer(outIndexBuffer,
                        0,
                        sizeof(ImDrawIdx) == 2 ? mesh_index_type::u16 : mesh_index_type::u32);

                    ImGuiViewport* const viewport = ctx.access(inViewport);
                    OBLO_ASSERT(viewport);

                    auto* const drawData = viewport->DrawData;

                    const vec2 scale = vec2::splat(2.f) / vec2{drawData->DisplaySize.x, drawData->DisplaySize.y};
                    const vec2 translation =
                        vec2::splat(-1.f) - scale * vec2{drawData->DisplayPos.x, drawData->DisplayPos.y};

                    const transform_constants transformConstants{
                        .scale = scale,
                        .translation = translation,
                    };

                    ctx.push_constants(shader_stage::vertex | shader_stage::fragment,
                        0,
                        as_bytes(std::span{&transformConstants, 1}));

                    // Will project scissor/clipping rectangles into framebuffer space
                    // (0,0) unless using multi-viewports
                    const ImVec2 clipOffset = drawData->DisplayPos;
                    // (1,1) unless using retina display which are often (2,2)
                    const ImVec2 clipScale = drawData->FramebufferScale;

                    i32 vertexOffset = 0;
                    i32 indexOffset = 0;

                    ImTextureID lastImage = ~ImTextureID{};

                    for (const ImDrawList* drawList : drawData->CmdLists)
                    {
                        for (const ImDrawCmd& cmd : drawList->CmdBuffer)
                        {
                            // Project scissor/clipping rectangles into framebuffer space
                            // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                            const vec2 clipMin{
                                max(0.f, (cmd.ClipRect.x - clipOffset.x) * clipScale.x),
                                max(0.f, (cmd.ClipRect.y - clipOffset.y) * clipScale.y),
                            };

                            const vec2 clipMax{
                                min((cmd.ClipRect.z - clipOffset.x) * clipScale.x, f32(cfg.renderResolution.x)),
                                min((cmd.ClipRect.w - clipOffset.y) * clipScale.y, f32(cfg.renderResolution.y)),
                            };

                            if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                            {
                                continue;
                            }

                            const ImTextureID newImage = cmd.GetTexID();

                            if (newImage != lastImage)
                            {
                                h32<vk::resident_texture> residentTexture{};

                                if (imgui_render_backend::is_subgraph_texture(newImage))
                                {
                                    const auto translatedId =
                                        imgui_render_backend::extract_subgraph_texture_index(newImage);

                                    residentTexture = subgraphTextures[translatedId];
                                }
                                else
                                {
                                    residentTexture = registeredTextures[newImage];
                                }

                                ctx.push_constants(shader_stage::vertex | shader_stage::fragment,
                                    sizeof(transform_constants),
                                    as_bytes(std::span{&residentTexture, 1}));

                                lastImage = newImage;
                            }

                            ctx.set_scissor(i32(clipMin.x),
                                i32(clipMin.y),
                                u32(clipMax.x - clipMin.x),
                                u32(clipMax.y - clipMin.y));

                            ctx.draw_indexed(cmd.ElemCount,
                                1,
                                cmd.IdxOffset + indexOffset,
                                cmd.VtxOffset + vertexOffset,
                                0);
                        }

                        indexOffset += drawList->IdxBuffer.Size;
                        vertexOffset += drawList->VtxBuffer.Size;
                    }

                    ctx.end_pass();
                }
            }
        };

        constexpr string_view g_InImGuiViewport{"ImGuiViewport"};
        constexpr string_view g_InImGuiBackend{"ImGuiBackend"};
        constexpr string_view g_InImGuiImageSink{"ImGuiImageSink"};
        constexpr string_view g_InOutRT{"RenderTarget"};

        constexpr string_view g_InImGuiId{"ImGuiId"};
        constexpr string_view g_InGraphTexture{"GraphTexture"};
        constexpr string_view g_OutImGuiImageSink{"ImGuiImageSink"};

        void create_imgui_frame_graph_templates(imgui_render_backend& backend)
        {
            vk::frame_graph_registry& registry = backend.nodeRegistry;
            registry.register_node<imgui_render_node>();
            registry.register_node<imgui_graph_image_push_node>();

            {
                auto& renderTemplate = backend.renderTemplate;
                renderTemplate.init(registry);

                auto render = renderTemplate.add_node<imgui_render_node>();
                renderTemplate.make_input(render, &imgui_render_node::inViewport, g_InImGuiViewport);
                renderTemplate.make_input(render, &imgui_render_node::inBackend, g_InImGuiBackend);
                renderTemplate.make_input(render, &imgui_render_node::inImageSink, g_InImGuiImageSink);
                renderTemplate.make_input(render, &imgui_render_node::inOutRenderTarget, g_InOutRT);
                renderTemplate.make_output(render, &imgui_render_node::inOutRenderTarget, g_InOutRT);
            }

            {
                auto& pushImageTemplate = backend.pushImageTemplate;
                pushImageTemplate.init(registry);

                auto push = pushImageTemplate.add_node<imgui_graph_image_push_node>();
                pushImageTemplate.make_input(push, &imgui_graph_image_push_node::inId, g_InImGuiId);
                pushImageTemplate.make_input(push, &imgui_graph_image_push_node::inTexture, g_InGraphTexture);
                pushImageTemplate.make_output(push, &imgui_graph_image_push_node::outImageSink, g_OutImGuiImageSink);
            }
        }

        imgui_render_userdata* create_render_userdata(const imgui_render_backend* backend,
            graphics_window_context* windowContext,
            bool isOwned,
            ImGuiViewport* viewport)
        {
            auto& frameGraph = *backend->frameGraph;

            auto graph = frameGraph.instantiate(backend->renderTemplate);
            frameGraph.set_input(graph, g_InImGuiViewport, viewport).assert_value();
            frameGraph.set_input(graph, g_InImGuiBackend, backend).assert_value();

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

            auto& frameGraph = *backend->frameGraph;
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

        expected<> init(const graphics_window& window, const imgui_app_config& cfg)
        {
            if (!window.is_ready())
            {
                return unspecified_error;
            }

            context = ImGui::CreateContext();

            auto& io = ImGui::GetIO();

            if (cfg.useDocking)
            {
                io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            }

            if (cfg.useMultiViewport)
            {
                io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
            }

            if (cfg.useKeyboardNavigation)
            {
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            }

            io.IniFilename = cfg.configFile;

            auto* const win32Window = get_win32_window(window);

            if (!ImGui_ImplWin32_Init(win32Window))
            {
                return unspecified_error;
            }

            return init_render_backend(window);
        }

        expected<> init_render_backend(const graphics_window& window)
        {
            auto& mm = module_manager::get();

            auto* const gfxEngine = mm.find_unique_service<graphics_engine>();

            if (!gfxEngine)
            {
                return unspecified_error;
            }

            auto* const vkEngine = mm.find<vk::vulkan_engine_module>();

            if (!vkEngine)
            {
                return unspecified_error;
            }

            ImGuiIO& io = ImGui::GetIO();
            OBLO_ASSERT(io.BackendRendererUserData == nullptr);

            backend.graphicsEngine = gfxEngine;
            backend.frameGraph = &vkEngine->get_frame_graph();
            create_imgui_frame_graph_templates(backend);

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

                if (rud)
                {
                    destroy_render_userdata(backend, rud);
                    viewport->RendererUserData = nullptr;
                }
            };

            platformIO.Renderer_SetWindowSize = [](ImGuiViewport* viewport, ImVec2 size)
            {
                auto* rud = static_cast<imgui_render_userdata*>(viewport->RendererUserData);
                rud->windowContext->on_resize(u32(size.x), u32(size.y));
            };

            return no_error;
        }

        void shutdown_renderer_backend()
        {
            ImGuiIO& io = ImGui::GetIO();

            if (io.BackendRendererUserData)
            {
                clear_push_subgraphs();
                io.BackendRendererUserData = nullptr;
            }
        }

        ~impl()
        {
            if (context)
            {
                ImGui::DestroyPlatformWindows();

                ImGui_ImplWin32_Shutdown();
                shutdown_renderer_backend();

                ImGui::DestroyContext(context);
                context = nullptr;
            }
        }

        void clear_push_subgraphs()
        {
            auto& fg = *backend.frameGraph;

            for (auto& sg : backend.pushImageSubgraphs)
            {
                fg.remove(sg);
            }

            backend.pushImageSubgraphs.clear();
        }
    };

    imgui_app::imgui_app() = default;

    imgui_app::~imgui_app() = default;

    expected<> imgui_app::init(const graphics_window_initializer& initializer, const imgui_app_config& cfg)
    {
        if (m_impl)
        {
            return unspecified_error;
        }

        if (auto e = graphics_app::init(initializer); !e)
        {
            return e;
        }

        m_eventProcessor.set_event_dispatcher({imgui_win32_dispatch_event});

        m_impl = allocate_unique<impl>();
        return m_impl->init(m_mainWindow, cfg);
    }

    void imgui_app::shutdown()
    {
        m_impl.reset();
        graphics_app::shutdown();
    }

    expected<> imgui_app::init_font_atlas(const resource_registry& resourceRegistry)
    {
        ImGuiIO& io = ImGui::GetIO();

        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        texture font;

        if (!font.allocate(texture_desc::make_2d(width, height, texture_format::r8g8b8a8_unorm)))
        {
            return unspecified_error;
        }

        std::span fontData = font.get_data();
        const usize expectedSize = width * height * sizeof(u32) * sizeof(char);

        if (fontData.size() != expectedSize)
        {
            return unspecified_error;
        }

        std::memcpy(fontData.data(), pixels, fontData.size_bytes());

        m_impl->backend.resourceRegistry = &resourceRegistry;
        m_impl->backend.register_texture(resourceRegistry.instantiate<texture>(std::move(font), "ImGui Font"));

        return no_error;
    }

    void imgui_app::begin_ui()
    {
        OBLO_PROFILE_SCOPE();

        m_impl->clear_push_subgraphs();

        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void imgui_app::end_ui()
    {
        OBLO_PROFILE_SCOPE();
        ImGui::Render();

        // Update and Render additional Platform Windows
        auto& io = ImGui::GetIO();

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
        }

        auto& fg = *m_impl->backend.frameGraph;

        auto& platformIO = ImGui::GetPlatformIO();

        for (auto* viewport : platformIO.Viewports)
        {
            if (viewport->RendererUserData)
            {
                connect_viewport(fg, viewport);
            }
        }
    }
}

namespace oblo::imgui
{
    ImTextureID add_image(resource_ref<texture> texture)
    {
        auto* backend = static_cast<imgui_render_backend*>(ImGui::GetIO().BackendRendererUserData);
        OBLO_ASSERT(backend);

        if (!backend)
        {
            return {};
        }

        return backend->register_texture(texture);
    }

    ImTextureID add_image(h32<vk::frame_graph_subgraph> subgraph, string_view output)
    {
        auto* viewport = ImGui::GetWindowViewport();
        OBLO_ASSERT(viewport);

        auto* backend = static_cast<imgui_render_backend*>(ImGui::GetIO().BackendRendererUserData);
        OBLO_ASSERT(backend);

        if (!viewport || !backend || !viewport->RendererUserData)
        {
            return {};
        }

        auto& frameGraph = *backend->frameGraph;
        auto* const rud = static_cast<imgui_render_userdata*>(viewport->RendererUserData);

        const auto pushGraph = frameGraph.instantiate(backend->pushImageTemplate);
        const ImTextureID newId = backend->register_subgraph_image(pushGraph);

        frameGraph.connect(subgraph, output, pushGraph, g_InGraphTexture);
        frameGraph.set_input(pushGraph, g_InImGuiId, newId).assert_value();

        frameGraph.connect(pushGraph, g_OutImGuiImageSink, rud->graph, g_InImGuiImageSink);

        backend->pushImageSubgraphs.push_back(pushGraph);

        return newId;
    }
}