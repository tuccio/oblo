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

bool intersect(in ray ray, in triangle triangle, in bool backfaceCulling, out float outDistance)
{
    const float Epsilon = .0000001f;

    const vec3 v0v1 = triangle.v[1] - triangle.v[0];
    const vec3 v0v2 = triangle.v[2] - triangle.v[0];

    const vec3 h = cross(ray.direction, v0v2);
    const float det = dot(v0v1, h);

    if (!backfaceCulling)
    {
        if (det > -Epsilon && det < Epsilon)
        {
            return false;
        }
    }
    else
    {
        if (det < Epsilon)
        {
            return false;
        }
    }

    const float invDet = 1.f / det;
    const vec3 s = ray.origin - triangle.v[0];

    const float u = invDet * dot(s, h);

    if (u < 0.f || u > 1.f)
    {
        return false;
    }

    const vec3 q = cross(s, v0v1);
    const float v = invDet * dot(ray.direction, q);

    if (v < 0.f || u + v > 1.f)
    {
        return false;
    }

    const float t = invDet * dot(v0v2, q);

    if (t > Epsilon)
    {
        outDistance = t;
        return true;
    }

    return false;
}

#endif