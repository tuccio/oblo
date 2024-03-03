#pragma once

#include <oblo/math/mat4.hpp>
#include <oblo/math/plane.hpp>

namespace oblo
{
    struct frustum
    {
        plane planes[6];
    };

    inline frustum normalize(const frustum& f)
    {
        frustum n;

        for (u32 i = 0; i < 6; ++i)
        {
            n.planes[i] = normalize(f.planes[i]);
        }

        return n;
    }

    inline frustum make_frustum_from_inverse_view_projection(const mat4& invViewProj)
    {
        constexpr auto unproject = [](const mat4& m, const vec4& v)
        {
            const vec4 h = m * v;
            return vec3{h.x, h.y, h.z} / h.w;
        };

        // Near is reversed due to reverse-Z in our projection matrix

        const vec3 tlf = unproject(invViewProj, {1.f, 1.f, 0.f, 1.f});
        const vec3 trf = unproject(invViewProj, {-1.f, 1.f, 0.f, 1.f});
        const vec3 blf = unproject(invViewProj, {1.f, -1.f, 0.f, 1.f});
        const vec3 brf = unproject(invViewProj, {-1.f, -1.f, 0.f, 1.f});
        const vec3 tln = unproject(invViewProj, {1.f, 1.f, 1.f, 1.f});
        const vec3 trn = unproject(invViewProj, {-1.f, 1.f, 1.f, 1.f});
        const vec3 bln = unproject(invViewProj, {1.f, -1.f, 1.f, 1.f});
        const vec3 brn = unproject(invViewProj, {-1.f, -1.f, 1.f, 1.f});

        frustum f;

        // near
        f.planes[0] = make_plane(tln, bln, brn);

        // far
        f.planes[1] = make_plane(tlf, brf, blf);

        // top
        f.planes[2] = make_plane(tln, trn, trf);

        // bottom
        f.planes[3] = make_plane(bln, brf, brn);

        // left
        f.planes[4] = make_plane(tln, blf, bln);

        // right
        f.planes[5] = make_plane(trn, brn, brf);

        return f;
    }
}