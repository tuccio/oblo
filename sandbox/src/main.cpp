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
        void load_texture(
            sf::Texture& texture, sf::Image& image, u16 width, u16 height, std::span<const vec3> colorBuffer)
        {
            auto it = colorBuffer.begin();

            for (auto y = 0u; y < height; ++y)
            {
                for (auto x = 0u; x < width; ++x)
                {
                    const auto [r, g, b] = *it;

                    const auto color =
                        sf::Color{narrow_cast<u8>(r * 255), narrow_cast<u8>(g * 255), narrow_cast<u8>(b * 255), 255};

                    image.setPixel(x, y, color);

                    ++it;
                }
            }

            texture.loadFromImage(image);
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

    sf::Image outImage;
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

    const auto onResize = [&](const auto& size)
    {
        const auto [width, height] = size;
        const auto fovy = f32{state.camera.fovx} * height / width;
        camera_set_vertical_fov(state.camera, radians{fovy});

        state.raytracerState->resize(narrow_cast<u16>(width), narrow_cast<u16>(height));

        outImage.create(width, height);
        outTexture.create(width, height);
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

        window.clear();

        if (!state.renderRasterized)
        {
            raytracer.render_debug(raytracerState, state.camera);

            const auto width = raytracerState.get_width();
            const auto height = raytracerState.get_height();

            load_texture(outTexture, outImage, width, height, raytracerState.get_radiance_buffer());

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