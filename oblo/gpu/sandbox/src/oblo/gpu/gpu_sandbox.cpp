#include <oblo/app/graphics_window.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/gpu/descriptors.hpp>
#include <oblo/gpu/gpu_instance.hpp>
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

            if (!m_gpu->create_device_and_queues(
                    {
                        .requireHardwareRaytracing = false,
                    },
                    m_windowSurface))
            {
                return "Failed to create GPU device"_err;
            }

            window_event_processor processor;

            while (m_window.is_open())
            {
                processor.process_events();
            }

            return no_error;
        }

        void shutdown()
        {
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
        graphics_window m_window;
        unique_ptr<gpu::gpu_instance> m_gpu;
        hptr<gpu::surface> m_windowSurface{};
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