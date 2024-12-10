#define SURFEL_STACK_QUALIFIER

#include <surfels/buffers/surfel_stack_buffer>

bool surfel_stack_allocate(out uint surfelId)
{
    const int prev = atomicAdd(g_SurfelStackHeader.available, -1);

    if (prev <= 0)
    {
        atomicAdd(g_SurfelStackHeader.available, 1);
        return false;
    }
    else
    {
        surfelId = g_SurfelStackEntries[prev - 1].surfelId;
        return true;
    }
}

void surfel_stack_free(in uint surfelI)
{
    // This is not safe if a pass can do both free and allocate, we need to be careful to allocate and free in different
    // passes
    const int newIndex = atomicAdd(g_SurfelStackHeader.available, 1);
    g_SurfelStackEntries[newIndex].surfelId = surfelI;
}