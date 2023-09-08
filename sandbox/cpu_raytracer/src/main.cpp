#include <oblo/raytracer/raytracer.hpp>
#include <sandbox/draw/debug_renderer.hpp>
#include <sandbox/draw/fullscreen_texture.hpp>
#include <sandbox/import/scene_importer.hpp>
#include <sandbox/state/config.hpp>
#include <sandbox/state/sandbox_state.hpp>
#include <sandbox/view/debug_view.hpp>

#include <GL/glew.h>
#include <SDL.h>

#include <cxxopts.hpp>

#include <imgui.h>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>

namespace oblo
{
    namespace
    {
        constexpr u16 s_tileSize{64};

        void update_tile(fullscreen_texture& texture,
                         u16 stride,
                         u16 minX,
                         u16 minY,
                         u16 maxX,
                         u16 maxY,
                         std::span<const vec3> colorBuffer,
                         u32 numSamples)
        {

            constexpr auto numChannels = 4;
            u8 buffer[s_tileSize * s_tileSize * numChannels];
            auto bufferIt = buffer;

            const auto correctAndNormalize = [weight = 1.f / numSamples](f32 color)
            {
                constexpr auto invGamma = 1.f / 2.2f;
                const auto radiance = color * weight;
                const auto tonemapped = radiance / (radiance + 1);
                return narrow_cast<u8>(std::pow(tonemapped, invGamma) * 255);
            };

            for (auto y = minY; y < maxY; ++y)
            {
                auto it = colorBuffer.begin() + stride * y + minX;

                for (auto x = minX; x < maxX; ++x)
                {
                    const auto [r, g, b] = *it;

                    *(bufferIt++) = correctAndNormalize(r);
                    *(bufferIt++) = correctAndNormalize(g);
                    *(bufferIt++) = correctAndNormalize(b);
                    *(bufferIt++) = 255;

                    ++it;
                }
            }

            texture.update_tile(buffer, minX, minY, maxX - minX, maxY - minY);
        }
    }
}

int main(int argc, char* argv[])
{
    using namespace oblo;

    constexpr auto configFile = "config.json";

    cxxopts::Options options{"oblo sandbox"};
    options.add_options()("import", "Import scene", cxxopts::value<std::filesystem::path>());

    const auto result = options.parse(argc, argv);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    auto* const window = SDL_CreateWindow("Oblo Ray-Tracing Sandbox",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          1280,
                                          720,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

    auto* const context = SDL_GL_CreateContext(window);

    if (glewInit() != GLEW_OK)
    {
        return glGetError();
    }

    sandbox_state state;

    if (!config_parse(configFile, state))
    {
        camera_set_look_at(state.camera, vec3{0.f, 0.f, -5.f}, vec3{0.f, 0.f, 1.f}, vec3{0.f, 1.f, 0.f});
        camera_set_horizontal_fov(state.camera, 90_deg);
        state.camera.near = 0.1f;
        state.camera.far = 100.f;
    }

    raytracer raytracer;
    raytracer_state raytracerState;

    debug_renderer debugRenderer;
    debug_view debugView;
    fullscreen_texture fullscreenTexture;

    state.raytracer = &raytracer;
    state.raytracerState = &raytracerState;
    state.debugRenderer = &debugRenderer;
    state.renderRasterized = false;

    if (result.count("import"))
    {
        state.latestImportedScene = result["import"].as<std::string>();

        scene_importer importer;
        importer.import(state, state.latestImportedScene);
    }
    else if (state.autoImportLastScene)
    {
        scene_importer importer;
        importer.import(state, state.latestImportedScene);
    }

    u16 tileX{0}, tileY{0};
    u16 numTilesX{0}, numTilesY{0};
    u32 renderWidth{0}, renderHeight{0};

    const auto onResize = [&](const u32 width, const u32 height)
    {
        const auto fovy = f32{state.camera.fovx} * height / width;
        camera_set_vertical_fov(state.camera, radians{fovy});

        state.raytracerState->resize(narrow_cast<u16>(width), narrow_cast<u16>(height), s_tileSize);

        fullscreenTexture.resize(width, height);

        renderWidth = width;
        renderHeight = height;

        numTilesX = round_up_div(u16(width), s_tileSize);
        numTilesY = round_up_div(u16(height), s_tileSize);

        tileX = 0;
        tileY = 0;
    };

    {
        int w, h;
        SDL_GL_GetDrawableSize(window, &w, &h);
        renderWidth = u32(w);
        renderHeight = u32(h);
    }

    onResize(renderWidth, renderHeight);

    auto* imguiContext = ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(window, context);

    if (!ImGui_ImplOpenGL3_Init("#version 450"))
    {
        return 1;
    }

    while (true)
    {
        for (SDL_Event event; SDL_PollEvent(&event);)
        {
            switch (event.type)
            {
            case SDL_QUIT:
                goto done;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    const auto width = u32(event.window.data1);
                    const auto height = u32(event.window.data2);
                    onResize(width, height);
                }

                break;
            }

            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        debugView.update(state);

        if (state.movedCamera)
        {
            state.movedCamera = false;
            state.raytracerState->reset_accumulation();
        }

        if (!state.renderRasterized)
        {
            const auto w = state.raytracerState->get_width();
            const auto h = state.raytracerState->get_height();

            const auto minX = tileX * s_tileSize;
            const auto maxX = min(w, u16(minX + s_tileSize));

            const auto minY = tileY * s_tileSize;
            const auto maxY = min(h, u16(minY + s_tileSize));

            raytracer.render_tile(raytracerState, state.camera, minX, minY, maxX, maxY);

            if (++tileX >= numTilesX)
            {
                tileX = 0;

                if (++tileY >= numTilesY)
                {
                    tileY = 0;
                }
            }

            update_tile(fullscreenTexture,
                        w,
                        minX,
                        minY,
                        maxX,
                        maxY,
                        raytracerState.get_radiance_buffer(),
                        raytracerState.get_num_samples_at(minX, minY));

            // Make sure we clear debug draws that we might have submitted even if we skip
            debugRenderer.clear();

            fullscreenTexture.draw();
        }
        else
        {
            debugRenderer.dispatch_draw(state);
        }

        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

done:

    if (state.writeConfigOnShutdown)
    {
        config_write(configFile, state);
    }

    ImGui::DestroyContext(imguiContext);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}