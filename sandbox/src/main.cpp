#include <imgui-SFML.h>

#include <oblo/rendering/raytracer.hpp>
#include <sandbox/draw/debug_renderer.hpp>
#include <sandbox/import/scene_importer.hpp>
#include <sandbox/state/config.hpp>
#include <sandbox/state/sandbox_state.hpp>
#include <sandbox/view/debug_view.hpp>

#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <cxxopts.hpp>
#include <imgui.h>

namespace oblo
{
    namespace
    {
        constexpr u16 s_tileSize{64};

        void update_tile([[maybe_unused]] sf::Texture& texture,
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

            const float weight = 1.f / numSamples;

            for (auto y = minY; y < maxY; ++y)
            {
                auto it = colorBuffer.begin() + stride * y + minX;

                for (auto x = minX; x < maxX; ++x)
                {
                    const auto [r, g, b] = *it;

                    *(bufferIt++) = narrow_cast<u8>(r * weight * 255);
                    *(bufferIt++) = narrow_cast<u8>(g * weight * 255);
                    *(bufferIt++) = narrow_cast<u8>(b * weight * 255);
                    *(bufferIt++) = 255;

                    ++it;
                }
            }

            texture.update(buffer, maxX - minX, maxY - minY, minX, minY);
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

    sf::ContextSettings contextSettings{24, 8};
    sf::RenderWindow window{sf::VideoMode{1280, 720}, "Sandbox", sf::Style::Default, contextSettings};
    ImGui::SFML::Init(window);

    if (glewInit() != GLEW_OK)
    {
        return 1;
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

    sf::Texture outTexture;

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

    const auto onResize = [&](const auto& size)
    {
        const auto [width, height] = size;
        const auto fovy = f32{state.camera.fovx} * height / width;
        camera_set_vertical_fov(state.camera, radians{fovy});

        state.raytracerState->resize(narrow_cast<u16>(width), narrow_cast<u16>(height), s_tileSize);

        outTexture.create(width, height);

        numTilesX = round_up_div(u16(width), s_tileSize);
        numTilesY = round_up_div(u16(height), s_tileSize);

        tileX = 0;
        tileY = 0;
    };

    window.setActive(true);
    onResize(window.getSize());

    sf::Clock deltaClock;

    while (window.isOpen())
    {
        sf::Event event;

        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Resized)
            {
                onResize(event.size);
            }

            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed)
            {
                window.close();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        debugView.update(state);

        if (state.movedCamera)
        {
            state.movedCamera = false;
            state.raytracerState->reset_accumulation();
        }

        window.clear();

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

            update_tile(outTexture,
                        w,
                        minX,
                        minY,
                        maxX,
                        maxY,
                        raytracerState.get_radiance_buffer(),
                        raytracerState.get_num_samples_at(minX, minY));

            sf::Sprite sprite{outTexture};
            window.draw(sprite);

            // Make sure we clear debug draws that we might have submitted even if we skip
            debugRenderer.clear();
        }
        else
        {
            debugRenderer.dispatch_draw(state);
        }

        ImGui::SFML::Render(window);
        window.resetGLStates();

        window.display();
    }

    if (state.writeConfigOnShutdown)
    {
        config_write(configFile, state);
    }

    return 0;
}