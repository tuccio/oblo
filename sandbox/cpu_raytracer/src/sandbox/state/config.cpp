#include <sandbox/state/config.hpp>

#include <sandbox/state/sandbox_state.hpp>

#include <nlohmann/json.hpp>

#include <fstream>

namespace oblo
{
    namespace
    {
        void parse(const nlohmann::json& json, std::string_view name, vec3& out)
        {
            if (const auto it = json.find(name); it != json.end() && it->is_array())
            {
                out.x = (*it)[0].get<f32>();
                out.y = (*it)[1].get<f32>();
                out.z = (*it)[2].get<f32>();
            }
        }

        template <typename T>
        void parse(const nlohmann::json& json, std::string_view name, T& out)
        {
            if (const auto it = json.find(name); it != json.end())
            {
                out = it->get<T>();
            }
        }

        void parse(const nlohmann::json& json, std::string_view name, radians& out)
        {
            f32 f32out;
            parse(json, name, f32out);
            out = radians{f32out};
        }

        auto to_json(vec3 value)
        {
            return nlohmann::json::array({value.x, value.y, value.z});
        }
    }

    bool config_parse(const char* filename, sandbox_state& state)
    {
        std::ifstream in{filename};

        if (!in)
        {
            return false;
        }

        const auto json = nlohmann::json::parse(in, nullptr, false);

        if (auto cameraIt = json.find("camera"); cameraIt != json.end())
        {
            parse(*cameraIt, "position", state.camera.position);
            parse(*cameraIt, "left", state.camera.left);
            parse(*cameraIt, "up", state.camera.up);
            parse(*cameraIt, "forward", state.camera.forward);
            parse(*cameraIt, "fovx", state.camera.fovx);
            parse(*cameraIt, "fovy", state.camera.fovy);
            parse(*cameraIt, "tanHalfFovX", state.camera.tanHalfFovX);
            parse(*cameraIt, "tanHalfFovY", state.camera.tanHalfFovY);
            parse(*cameraIt, "near", state.camera.near);
            parse(*cameraIt, "far", state.camera.far);
        }

        parse(json, "rasterize", state.renderRasterized);
        parse(json, "writeConfigOnShutdown", state.writeConfigOnShutdown);
        parse(json, "autoImportLastScene", state.autoImportLastScene);
        parse(json, "latestImportedScene", state.latestImportedScene);

        return true;
    }

    bool config_write(const char* filename, const sandbox_state& state)
    {
        nlohmann::ordered_json json;

        {
            auto& camera = json["camera"];

            camera["position"] = to_json(state.camera.position);
            camera["left"] = to_json(state.camera.left);
            camera["up"] = to_json(state.camera.up);
            camera["forward"] = to_json(state.camera.forward);
            camera["fovx"] = f32{state.camera.fovx};
            camera["fovy"] = f32{state.camera.fovy};
            camera["tanHalfFovX"] = state.camera.tanHalfFovX;
            camera["tanHalfFovY"] = state.camera.tanHalfFovY;
            camera["near"] = state.camera.near;
            camera["far"] = state.camera.far;
        }

        json["rasterize"] = state.renderRasterized;
        json["writeConfigOnShutdown"] = state.writeConfigOnShutdown;
        json["autoImportLastScene"] = state.autoImportLastScene;
        json["latestImportedScene"] = state.latestImportedScene;

        std::ofstream out{filename};
        out << json.dump(1, '\t');

        return true;
    }
}