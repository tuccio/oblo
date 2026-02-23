#include <oblo/app/graphics_window.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/error.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/vulkan/vulkan_instance.hpp>

#include <chrono>
#include <cstdio>
#include <thread>

namespace oblo
{
    class gpu_sandbox
    {
    public:
        ~gpu_sandbox()
        {
            shutdown();
        }

        expected<> run()
        {
            m_gpu = allocate_unique<gpu::vk::vulkan_instance>();

            if (!m_gpu->init({
                    .application = "GPU Sandbox",
                    .engine = "oblo",
                }))
            {
                return "Failed to initialize GPU instance"_err;
            }

            if (!m_window.create({
                    .title = "GPU Sandbox",
                    .style = window_style::app,
                }))
            {
                return "Failed to create window"_err;
            }

            const hptr nativeWindowHandle = std::bit_cast<hptr<gpu::native_window>>(m_window.get_native_handle());

            if (const expected surface = m_gpu->create_surface(nativeWindowHandle); !surface)
            {
                return "Failed to create surface"_err;
            }
            else
            {
                m_windowSurface = *surface;
            }

            if (!m_gpu->finalize_init(
                    {
                        .requireHardwareRaytracing = false,
                    },
                    m_windowSurface))
            {
                return "Failed to create GPU device"_err;
            }

            for (u32 i = 0; i < num_swapchain_images; ++i)
            {
                const auto sem = m_gpu->create_semaphore({});

                if (!sem)
                {
                    return "Failed to create semaphore"_err;
                }

                m_semaphores[i] = *sem;

                const auto pool = m_gpu->create_command_buffer_pool({
                    .queue = m_gpu->get_universal_queue(),
                    .numCommandBuffers = 1u,
                });

                if (!pool)
                {
                    return "Failed to create command buffer pool"_err;
                }

                m_commandBufferPools[i] = *pool;
            }

            vec2u lastWindowSize{};

            window_event_processor processor;

            u32 semaphoreIndex = 0u;

            while (m_window.is_open())
            {
                processor.process_events();

                const vec2u windowSize = m_window.get_size();

                if (!m_swapchain || (windowSize.x != lastWindowSize.x || windowSize.y != lastWindowSize.y))
                {
                    recreate_swapchain(windowSize);

                    if (!m_swapchain)
                    {
                        continue;
                    }

                    lastWindowSize = windowSize;
                }

                h32<gpu::image> backBuffer{};

                if (!m_gpu->begin_submit_tracking())
                {
                    return "Failed to initialize queue submission"_err;
                }

                const h32<gpu::semaphore> currentFrameSemaphore = m_semaphores[semaphoreIndex];

                do
                {
                    const expected result = m_gpu->acquire_swapchain_image(m_swapchain, currentFrameSemaphore);

                    if (result)
                    {
                        backBuffer = result.value();
                        break;
                    }

                    if (result.error() == gpu::error::out_of_date)
                    {
                        recreate_swapchain(windowSize);
                    }
                } while (true);

                const h32 pool = m_commandBufferPools[semaphoreIndex];

                if (!m_gpu->reset_command_buffer_pool(pool))
                {
                    return "Failed to reset command buffer pool"_err;
                }

                // TODO: Do something with the back buffer

                hptr<gpu::command_buffer> commandBuffers[1];

                if (!m_gpu->fetch_command_buffers(pool, commandBuffers))
                {
                    return "Failed to fetch command buffers"_err;
                }

                const handle cmd = commandBuffers[0];

                if (!m_gpu->begin_command_buffer(cmd))
                {
                    return "Failed to begin command buffer"_err;
                }

                render(cmd, backBuffer);

                if (!m_gpu->end_command_buffer(cmd))
                {
                    return "Failed to end command buffer"_err;
                }

                if (!m_gpu->submit(m_gpu->get_universal_queue(),
                        {
                            .commandBuffers = commandBuffers,
                            .signalSemaphores = {&currentFrameSemaphore, 1u},
                        }))
                {
                    return "Failed to submit command buffers"_err;
                }

                if (const expected r = m_gpu->present({
                        .swapchains = {&m_swapchain, 1u},
                        .waitSemaphores = {&currentFrameSemaphore, 1u},
                    });
                    !r && r.error() != gpu::error::out_of_date)
                {
                    return "Failed to present"_err;
                }

                semaphoreIndex = (semaphoreIndex + 1) % num_swapchain_images;
            }

            return no_error;
        }

        void shutdown()
        {
            if (m_gpu)
            {
                m_gpu->wait_idle().assert_value();

                for (u32 i = 0; i < num_swapchain_images; ++i)
                {
                    if (m_semaphores[i])
                    {
                        m_gpu->destroy_semaphore(m_semaphores[i]);
                        m_semaphores[i] = {};
                    }

                    if (m_commandBufferPools[i])
                    {
                        m_gpu->destroy_command_buffer_pool(m_commandBufferPools[i]);
                        m_commandBufferPools[i] = {};
                    }
                }

                if (m_swapchain)
                {
                    m_gpu->destroy_swapchain(m_swapchain);
                    m_swapchain = {};
                }

                if (m_windowSurface)
                {
                    m_gpu->destroy_surface(m_windowSurface);
                    m_windowSurface = {};
                }

                m_gpu->shutdown();
                m_gpu.reset();
            }

            m_window.destroy();
        }

    private:
        void render(hptr<gpu::command_buffer> cmd, h32<gpu::image> swapchainImage)
        {
            if (!ensure_pipelines_exist())
            {
                return;
            }

            {
                // Prepare to present

                const gpu::image_state_transition imageBarriers[] = {
                    {
                        .image = swapchainImage,
                        .previousState = gpu::image_resource_state::undefined,
                        .nextState = gpu::image_resource_state::render_target_write,
                        .previousPipeline = gpu::pipeline_sync_stage::top_of_pipeline,
                        .nextPipeline = gpu::pipeline_sync_stage::graphics,
                    },
                };

                const gpu::memory_barrier_descriptor barriers{
                    .images = imageBarriers,
                };

                m_gpu->cmd_apply_barriers(cmd, barriers);
            }

            const gpu::render_attachment colorAttachments[1] = {{
                .image = swapchainImage,
                .loadOp = gpu::attachment_load_op::clear,
                .storeOp = gpu::attachment_store_op::store,
                .clearValue =
                    {
                        .color = {.f32 = {1.f, 0.f, 0.f, 1.f}},
                    },
            }};

            const gpu::render_pass_descriptor renderPassDesc{
                .colorAttachments = colorAttachments,
            };

            if (const auto r = m_gpu->begin_render_pass(cmd, m_renderPipeline, renderPassDesc))
            {
                m_gpu->end_render_pass(cmd, *r);
            }

            {
                // Prepare to present

                const gpu::image_state_transition imageBarriers[] = {
                    {
                        .image = swapchainImage,
                        .previousState = gpu::image_resource_state::render_target_write,
                        .nextState = gpu::image_resource_state::present,
                        .previousPipeline = gpu::pipeline_sync_stage::graphics,
                        .nextPipeline = gpu::pipeline_sync_stage::bottom_of_pipeline,
                    },
                };

                const gpu::memory_barrier_descriptor barriers{
                    .images = imageBarriers,
                };

                m_gpu->cmd_apply_barriers(cmd, barriers);
            }
        }

    private:
        void recreate_swapchain(vec2u windowSize)
        {
            if (m_swapchain)
            {
                m_gpu->wait_idle().assert_value();
                m_gpu->destroy_swapchain(m_swapchain);
            }

            m_swapchain = m_gpu
                              ->create_swapchain({
                                  .surface = m_windowSurface,
                                  .numImages = num_swapchain_images,
                                  .format = gpu::image_format::b8g8r8a8_unorm,
                                  .width = windowSize.x,
                                  .height = windowSize.y,
                              })
                              .value_or({});
        }

        bool ensure_pipelines_exist()
        {
            if (!m_renderPipeline)
            {
                const gpu::image_format rtFormats[1] = {gpu::image_format::b8g8r8a8_unorm};

                const gpu::render_pass_targets targets{
                    .colorAttachmentFormats = rtFormats,
                };

                const gpu::render_pipeline_descriptor desc{
                    .renderTargets = targets,
                };

                const expected r = m_gpu->create_render_pipeline(desc);
                m_renderPipeline = r.value_or({});
            }

            return bool{m_renderPipeline};
        }

    private:
        static constexpr u32 num_swapchain_images = 3u;

    private:
        graphics_window m_window;
        unique_ptr<gpu::gpu_instance> m_gpu;
        hptr<gpu::surface> m_windowSurface{};
        h32<gpu::swapchain> m_swapchain{};
        h32<gpu::semaphore> m_semaphores[num_swapchain_images]{};
        h32<gpu::command_buffer_pool> m_commandBufferPools[num_swapchain_images]{};
        h32<gpu::render_pipeline> m_renderPipeline{};
    };
}

int main(int, char*[])
{
    oblo::gpu_sandbox sandbox;

    const auto e = sandbox.run();

    if (!e)
    {
        std::fputs(e.error().message, stderr);
    }

    return 0;
}