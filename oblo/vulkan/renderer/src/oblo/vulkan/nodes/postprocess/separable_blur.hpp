#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>

#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>

namespace oblo::vk
{
    template <typename T>
    concept separable_blur_config = requires(T cfg, dynamic_array<f32>& a) {
        {
            make_separable_blur_kernel(cfg, a)
        };
        {
            string_view{T::get_shader_name()}
        };
    };

    enum class separable_blur_pass
    {
        horizontal,
        vertical
    };

    template <separable_blur_config Config, separable_blur_pass Pass>
    struct separable_blur
    {
        resource<texture> inSource;

        resource<texture> outBlurred;

        data<Config> inConfig;

        h32<compute_pass> blurPass;

        std::span<const f32> kernel;

        bool outputInPlace{true};

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}