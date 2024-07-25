#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/types.hpp>

#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>
#include <string_view>

namespace oblo::vk
{
    template <typename T>
    concept separable_blur_config = requires(T cfg, dynamic_array<f32>& a) {
        {
            make_separable_blur_kernel(cfg, a)
        };
        {
            std::string_view{T::get_shader_name()}
        };
    };

    template <separable_blur_config Config, u8 PassIndex>
    struct separable_blur
    {
        resource<texture> inSource;

        resource<texture> outBlurred;

        data<Config> inConfig;

        h32<compute_pass> blurPass;

        std::span<const f32> kernel;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}