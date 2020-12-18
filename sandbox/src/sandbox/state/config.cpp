#include <sandbox/state/config.hpp>

#include <sandbox/state/sandbox_state.hpp>

#include <fstream>

#pragma clang diagnostic ignored "-Wambiguous-reversed-operator"

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>

#define OBLO_TRY_SERIALIZE(Name, Member)                                                                               \
    try                                                                                                                \
    {                                                                                                                  \
        ar(cereal::make_nvp(Name, Member));                                                                            \
    }                                                                                                                  \
    catch (const std::exception&)                                                                                      \
    {                                                                                                                  \
    }

namespace oblo
{
    inline void prologue(cereal::JSONOutputArchive&, const radians&) {}
    inline void epilogue(cereal::JSONOutputArchive&, const radians&) {}

    inline void prologue(cereal::JSONInputArchive&, radians&) {}
    inline void epilogue(cereal::JSONInputArchive&, radians&) {}

    template <typename Archive>
    void load(Archive& ar, radians& r)
    {
        f32 value;
        ar(value);
        r = radians{value};
    }

    template <typename Archive>
    void save(Archive& ar, const radians& r)
    {
        ar(f32{r});
    }

    template <typename Archive>
    void serialize(Archive& ar, vec3& v)
    {
        std::size_t size{3};
        ar(cereal::make_size_tag(size));
        ar(v.x, v.y, v.z);
    }

    template <typename Archive>
    void serialize(Archive& ar, camera& camera)
    {
        OBLO_TRY_SERIALIZE("position", camera.position);
        OBLO_TRY_SERIALIZE("left", camera.left);
        OBLO_TRY_SERIALIZE("up", camera.up);
        OBLO_TRY_SERIALIZE("forward", camera.forward);
        OBLO_TRY_SERIALIZE("fovx", camera.fovx);
        OBLO_TRY_SERIALIZE("fovy", camera.fovy);
        OBLO_TRY_SERIALIZE("tanHalfFovX", camera.tanHalfFovX);
        OBLO_TRY_SERIALIZE("tanHalfFovY", camera.tanHalfFovY);
        OBLO_TRY_SERIALIZE("near", camera.near);
        OBLO_TRY_SERIALIZE("far", camera.far);
    }

    inline void prologue(cereal::JSONOutputArchive&, const sandbox_state&) {}
    inline void epilogue(cereal::JSONOutputArchive&, const sandbox_state&) {}

    inline void prologue(cereal::JSONInputArchive&, sandbox_state&) {}
    inline void epilogue(cereal::JSONInputArchive&, sandbox_state&) {}

    template <typename Archive>
    void serialize(Archive& ar, sandbox_state& state)
    {
        OBLO_TRY_SERIALIZE("camera", state.camera);
        OBLO_TRY_SERIALIZE("rasterize", state.renderRasterized);
        OBLO_TRY_SERIALIZE("writeConfigOnShutdown", state.writeConfigOnShutdown);
        OBLO_TRY_SERIALIZE("latestImportedScene", state.latestImportedScene);
    }

    bool config_parse(const char* filename, sandbox_state& state)
    {
        std::ifstream in{filename};

        if (!in)
        {
            return false;
        }

        cereal::JSONInputArchive ar{in};

        try
        {
            ar(state);
        }
        catch (const std::exception&)
        {
            return false;
        }

        return true;
    }

    bool config_write(const char* filename, const sandbox_state& state)
    {
        std::ofstream out{filename};

        if (!out)
        {
            return false;
        }

        cereal::JSONOutputArchive ar{out};

        try
        {
            ar(state);
        }
        catch (const std::exception&)
        {
            return false;
        }

        return true;
    }
}