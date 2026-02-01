#include <oblo/app/graphics_window.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/gpu/descriptors.hpp>
#include <oblo/gpu/error.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/types.hpp>
#include <oblo/gpu/vulkan/vulkan_instance.hpp>

#include <cstdio>

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
            m_gpu = allocate_unique<gpu::vulkan_instance>();

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
            }

            vec2u lastWindowSize{};

            window_event_processor processor;

            u32 semaphoreIndex = 0u;

            while (m_window.is_open())
            {
                processor.process_events();

                const vec2u windowSize = m_window.get_size();

                if (!m_swapchain || (windowSize.x != lastWindowSize.x || lastWindowSize.y != lastWindowSize.y))
                {
                    recreate_swapchain(windowSize);
                    lastWindowSize = windowSize;
                }

                h32<gpu::image> backBuffer{};

                do
                {
                    const expected result = m_gpu->acquire_swapchain_image(m_swapchain, m_semaphores[semaphoreIndex]);

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

                // TODO: Do something with the back buffer
            }

            return no_error;
        }

        void shutdown()
        {
            if (m_swapchain)
            {
                m_gpu->wait_idle().assert_value();
                m_gpu->destroy_swapchain(m_swapchain);
            }

            if (m_windowSurface)
            {
                m_gpu->destroy_surface(m_windowSurface);
                m_windowSurface = {};
            }

            if (m_gpu)
            {
                m_gpu->shutdown();
                m_gpu.reset();
            }

            m_window.destroy();
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
                                  .format = gpu::texture_format::b8g8r8a8_unorm,
                                  .width = windowSize.x,
                                  .height = windowSize.y,
                              })
                              .value_or({});
        }

    private:
        static constexpr u32 num_swapchain_images = 3u;

    private:
        graphics_window m_window;
        unique_ptr<gpu::gpu_instance> m_gpu;
        hptr<gpu::surface> m_windowSurface{};
        h32<gpu::swapchain> m_swapchain{};
        h32<gpu::semaphore> m_semaphores[num_swapchain_images]{};
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