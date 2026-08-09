#ifndef OBLO_INCLUDE_RENDERER_COMPUTE_UTILITY
#define OBLO_INCLUDE_RENDERER_COMPUTE_UTILITY

uint calculate_global_invocation_index()
{
    const uint width = gl_NumWorkGroups.x * gl_WorkGroupSize.x;
    const uint height = gl_NumWorkGroups.y * gl_WorkGroupSize.y;
    return gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * width + gl_GlobalInvocationID.z * width * height;
}

#endif
