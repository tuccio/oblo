#include <imgui-SFML.h>

#include <sandbox/draw/debug_renderer.hpp>
#include <sandbox/sandbox_state.hpp>
#include <sandbox/view/debug_view.hpp>

#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <imgui.h>

int main(int argc, char* argv[])
{
    using namespace oblo;

    sf::ContextSettings contextSettings{24, 8};
    sf::RenderWindow window{sf::VideoMode{1280, 720}, "Sandbox", sf::Style::Default, contextSettings};
    ImGui::SFML::Init(window);

    if (glewInit() != GLEW_OK)
    {
        return 1;
    }

    window.resetGLStates();

    sandbox_state state;

    debug_renderer debugRenderer;
    debug_view debugView;

    state.debugRenderer = &debugRenderer;
    state.renderRasterized = true;
    state.camera.position = vec3{0.f, 0.f, -5.f};
    state.camera.up = vec3{0.f, 1.f, 0.f};
    state.camera.direction = vec3{0.f, 0.f, 1.f};
    state.camera.fovx = 90_deg;
    state.camera.fovy = 50.6_deg;
    state.camera.near = 0.1f;
    state.camera.far = 100.f;

    window.setActive(true);

    sf::Clock deltaClock;

    while (window.isOpen())
    {
        sf::Event event;

        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Resized)
            {
                const auto fovy = float{state.camera.fovx} * event.size.height / event.size.width;
                state.camera.fovy = radians{fovy};
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

        debugRenderer.dispatch_draw(state);

        ImGui::SFML::Render(window);
        window.display();
    }

    return 0;
}