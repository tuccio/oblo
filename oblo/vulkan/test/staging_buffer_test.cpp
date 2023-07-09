#include <gtest/gtest.h>

#include <oblo/core/finally.hpp>
#include <oblo/core/small_vector.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/staging_buffer.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <array>
#include <numeric>
#include <sstream>

#define ASSERT_VK_SUCCESS(...) ASSERT_EQ(VK_SUCCESS, __VA_ARGS__)

namespace oblo::vk
{
    namespace
    {
        VKAPI_ATTR VkBool32 VKAPI_CALL
        validation_cb([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                      [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
                      [[maybe_unused]] const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                      void* pUserData)
        {
            auto& errors = *static_cast<std::stringstream*>(pUserData);

            errors << "[Vulkan Validation] (" << std::hex << messageType << ") " << pCallbackData->pMessage;
            return VK_FALSE;
        }

        class test_sandbox
        {
        public:
            test_sandbox() = default;
            test_sandbox(const test_sandbox&) = delete;
            test_sandbox(test_sandbox&&) noexcept = delete;
            test_sandbox& operator=(const test_sandbox&) = delete;
            test_sandbox& operator=(test_sandbox&&) noexcept = delete;

            ~test_sandbox()
            {
                shutdown();
            }

            bool init(std::span<const char* const> instanceExtensions,
                      std::span<const char* const> instanceLayers,
                      std::span<const char* const> deviceExtensions,
                      void* deviceFeaturesList,
                      const VkPhysicalDeviceFeatures* physicalDeviceFeatures,
                      std::stringstream* validationErrors)
            {
                if (!create_window() || !create_engine(instanceExtensions,
                                                       instanceLayers,
                                                       deviceExtensions,
                                                       deviceFeaturesList,
                                                       physicalDeviceFeatures,
                                                       validationErrors))
                {
                    return false;
                }

                return true;
            }

            void shutdown()
            {
                if (surface)
                {
                    vkDestroySurfaceKHR(instance.get(), surface, nullptr);
                }

                if (window)
                {
                    SDL_DestroyWindow(window);
                    window = nullptr;
                }

                engine.shutdown();
                instance.shutdown();
            }

        private:
            bool create_window()
            {
                window = SDL_CreateWindow("Oblo Vulkan Test",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          1280,
                                          720,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

                return window != nullptr;
            }

            bool create_engine(std::span<const char* const> instanceExtensions,
                               std::span<const char* const> instanceLayers,
                               std::span<const char* const> deviceExtensions,
                               void* deviceFeaturesList,
                               const VkPhysicalDeviceFeatures* physicalDeviceFeatures,
                               std::stringstream* validationErrors)
            {
                // We need to gather the extensions needed by SDL
                constexpr u32 extensionsArraySize{64};
                constexpr u32 layersArraySize{16};

                small_vector<const char*, extensionsArraySize> extensions;
                small_vector<const char*, layersArraySize> layers;

                u32 sdlExtensionsCount;

                if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionsCount, nullptr))
                {
                    return false;
                }

                extensions.resize(sdlExtensionsCount);

                if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionsCount, extensions.data()))
                {
                    return false;
                }

                layers.emplace_back("VK_LAYER_KHRONOS_validation");

                extensions.insert(extensions.end(), instanceExtensions.begin(), instanceExtensions.end());
                layers.insert(layers.end(), instanceLayers.begin(), instanceLayers.end());

                constexpr u32 apiVersion{VK_API_VERSION_1_3};

                if (!instance.init(
                        VkApplicationInfo{
                            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .pNext = nullptr,
                            .pApplicationName = "vksandbox",
                            .applicationVersion = 0,
                            .pEngineName = "oblo",
                            .engineVersion = 0,
                            .apiVersion = apiVersion,
                        },
                        {layers},
                        {extensions},
                        validation_cb,
                        validationErrors))
                {
                    return false;
                }

                if (!SDL_Vulkan_CreateSurface(window, instance.get(), &surface))
                {
                    return false;
                }

                constexpr const char* internalDeviceExtensions[] = {
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
                    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
                    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                };

                extensions.assign(std::begin(internalDeviceExtensions), std::end(internalDeviceExtensions));
                extensions.insert(extensions.end(), deviceExtensions.begin(), deviceExtensions.end());

                VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
                    .pNext = deviceFeaturesList,
                    .dynamicRendering = VK_TRUE,
                };

                VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature{
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
                    .pNext = &dynamicRenderingFeature,
                    .timelineSemaphore = VK_TRUE,
                };

                VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeature{
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
                    .pNext = &timelineFeature,
                    .bufferDeviceAddress = VK_TRUE,
                };

                return engine.init(instance.get(),
                                   surface,
                                   {},
                                   {extensions},
                                   &bufferDeviceAddressFeature,
                                   physicalDeviceFeatures);
            }

        public:
            SDL_Window* window;
            VkSurfaceKHR surface{nullptr};

            instance instance;
            single_queue_engine engine;
        };
    }

    TEST(staging_buffer, staging_buffer)
    {
        constexpr u32 bufferSize{64u};
        constexpr u32 stagingBufferSize{128u};
        constexpr u32 buffersCount{4};

        std::stringstream validationErrors;

        {
            test_sandbox sandbox;
            ASSERT_TRUE(sandbox.init({}, {}, {}, nullptr, nullptr, &validationErrors));

            allocator allocator;
            ASSERT_TRUE(allocator.init(sandbox.instance.get(),
                                       sandbox.engine.get_physical_device(),
                                       sandbox.engine.get_device()));

            staging_buffer stagingBuffer;
            ASSERT_TRUE(stagingBuffer.initialize(sandbox.engine, allocator, stagingBufferSize));

            // Create the buffers
            allocated_buffer buffers[buffersCount];
            void* mappings[buffersCount];
            VmaAllocation allocations[buffersCount];

            for (u32 index = 0u; index < buffersCount; ++index)
            {
                ASSERT_VK_SUCCESS(allocator.create_buffer(
                    {
                        .size = bufferSize,
                        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        .memoryUsage = memory_usage::gpu_to_cpu,
                    },
                    buffers + index));

                ASSERT_VK_SUCCESS(allocator.map(buffers[index].allocation, mappings + index));

                allocations[index] = buffers[index].allocation;
            }

            const auto cleanup = finally(
                [&allocator, &buffers]
                {
                    for (auto buffer : buffers)
                    {
                        allocator.destroy(buffer);
                    }
                });

            using data_array = std::array<i32, bufferSize / sizeof(i32)>;

            data_array data;
            std::iota(std::begin(data), std::end(data), 0);

            const auto dataSpan = std::as_bytes(std::span{data});

            // The first uploads will work, after that we need to flush because we used the whole staging buffer
            ASSERT_TRUE(stagingBuffer.upload(dataSpan, buffers[0].buffer, 0));
            ASSERT_TRUE(stagingBuffer.upload(dataSpan, buffers[1].buffer, 0));
            ASSERT_FALSE(stagingBuffer.upload(dataSpan, buffers[2].buffer, 0));
            ASSERT_FALSE(stagingBuffer.upload(dataSpan, buffers[3].buffer, 0));

            stagingBuffer.flush();
            vkQueueWaitIdle(sandbox.engine.get_queue());

            ASSERT_VK_SUCCESS(allocator.invalidate_mapped_memory_ranges(allocations));

            auto readbackBuffer = [](void* mapping)
            {
                data_array readback;
                std::memcpy(&readback, mapping, sizeof(readback));
                return readback;
            };

            ASSERT_EQ(readbackBuffer(mappings[0]), data);
            ASSERT_EQ(readbackBuffer(mappings[1]), data);

            stagingBuffer.wait_for_free_space(stagingBufferSize);

            // Now we do it the other way around, so it will be 0 and 1 to fail
            ASSERT_TRUE(stagingBuffer.upload(dataSpan, buffers[3].buffer, 0));
            ASSERT_TRUE(stagingBuffer.upload(dataSpan, buffers[2].buffer, 0));
            ASSERT_FALSE(stagingBuffer.upload(dataSpan, buffers[1].buffer, 0));
            ASSERT_FALSE(stagingBuffer.upload(dataSpan, buffers[0].buffer, 0));

            stagingBuffer.flush();
            vkQueueWaitIdle(sandbox.engine.get_queue());

            ASSERT_VK_SUCCESS(allocator.invalidate_mapped_memory_ranges(allocations));

            ASSERT_EQ(readbackBuffer(mappings[2]), data);
            ASSERT_EQ(readbackBuffer(mappings[3]), data);
        }

        const auto errorsString = validationErrors.str();
        ASSERT_TRUE(errorsString.empty()) << errorsString;
    }
}