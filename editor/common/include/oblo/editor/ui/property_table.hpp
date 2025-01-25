#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    struct quaternion;
    struct vec2;
    struct vec3;
    struct vec4;
    struct uuid;
}

namespace oblo::editor::ui
{
    class artifact_picker;

    namespace property_table
    {
        using id_t = i32;

        [[nodiscard]] bool begin();
        void end();

        void add_empty(cstring_view name);

        bool add(id_t id, cstring_view name, bool& v);
        bool add(id_t id, cstring_view name, u32& v);
        bool add(id_t id, cstring_view name, f32& v);

        bool add(id_t id, cstring_view name, vec2& v);
        bool add(id_t id, cstring_view name, vec3& v);
        bool add(id_t id, cstring_view name, vec4& v);

        bool add(id_t id, cstring_view name, quaternion& v);

        bool add_color(id_t id, cstring_view name, vec3& v);

        bool add(id_t id, cstring_view name, uuid& anyUuid);
        bool add(id_t id, cstring_view name, uuid& artifactId, artifact_picker& picker, const uuid& typeUuid);
    };
}