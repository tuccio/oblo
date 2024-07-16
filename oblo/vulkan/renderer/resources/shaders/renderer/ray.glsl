#ifndef OBLO_INCLUDE_RENDERER_RAYTRACE
#define OBLO_INCLUDE_RENDERER_RAYTRACE

struct triangle
{
    vec3 v[3];
};

struct ray
{
    vec3 origin;
    vec3 direction;
};

bool distance_from_triangle_plane(in ray ray, in triangle triangle, out float outDistance)
{
    const float Epsilon = .0000001f;

    const vec3 v0v1 = triangle.v[1] - triangle.v[0];
    const vec3 v0v2 = triangle.v[2] - triangle.v[0];

    const vec3 h = cross(ray.direction, v0v2);
    const float det = dot(v0v1, h);

    if (det > -Epsilon && det < Epsilon)
    {
        return false;
    }

    const float invDet = 1.f / det;
    const vec3 s = ray.origin - triangle.v[0];

    const vec3 q = cross(s, v0v1);

    outDistance = invDet * dot(v0v2, q);

    return true;
}

vec3 ray_point_at(in ray ray, in float t)
{
    return ray.origin + ray.direction * t;
}

#endif