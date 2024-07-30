#include <oblo/vulkan/data/blur_configs.hpp>

#include <oblo/math/constants.hpp>

#include <cmath>

namespace oblo::vk
{
    void make_separable_blur_kernel(const gaussian_blur_config& cfg, dynamic_array<f32>& outKernel)
    {
        const u32 kernelDataSize = 1 + cfg.kernelSize / 2;
        outKernel.assign_default(kernelDataSize);

        const f32 sigma2 = cfg.sigma * cfg.sigma;
        const f32 G = 1.f / std::sqrt(2.f * pi * sigma2);
        const f32 k = -.5f / sigma2;

        for (u32 i = 0; i < kernelDataSize; ++i)
        {
            outKernel[i] = G * std::exp(k * (i * i));
        }
    }
}