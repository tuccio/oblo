#ifndef OBLO_INCLUDE_RENDERER_VOLUMES
#define OBLO_INCLUDE_RENDERER_VOLUMES

struct padded_aabb
{
    vec3 min;
    float _padding0;
    vec3 max;
    float _padding1;
};

struct aabb
{
    vec3 min;
    vec3 max;
};

// Represent a plane equation in the form: dot(normal, v) + offset = 0
struct plane
{
    vec3 normal;
    float offset;
};

struct frustum
{
    plane planes[6];
};

plane make_plane(in vec3 p1, in vec3 p2, in vec3 p3)
{
    const vec3 v12 = p2 - p1;
    const vec3 v13 = p3 - p1;

    plane r;
    r.normal = normalize(cross(v12, v13));
    r.offset = -dot(r.normal, p1);
    return r;
}

#endif
