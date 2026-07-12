#ifndef OBLO_INCLUDE_SURFELS_SURFEL_GRID_BUFFER
#define OBLO_INCLUDE_SURFELS_SURFEL_GRID_BUFFER

#include <renderer/utility/hash_map>
#include <surfels/buffers/surfel_buffer_bindings>
#include <surfels/surfel_data>

#if !defined(SURFEL_GRID_BINDING) || !defined(SURFEL_GRID_HASH_BINDING) || !defined(SURFEL_GRID_QUALIFIER)
    #error "Binding and memory qualifier must be defined before including this header"
#endif

layout(std430, binding = SURFEL_GRID_BINDING) restrict SURFEL_GRID_QUALIFIER buffer b_SurfelsGrid
{
    // TODO: Should probably split in 2 different buffers
    surfel_grid_header g_SurfelGridHeader;
    surfel_grid_cell g_SurfelGridCells[];
};

layout(std430, binding = SURFEL_GRID_HASH_BINDING) restrict SURFEL_GRID_QUALIFIER buffer b_SurfelsGridHashMap
{
    // TODO: Should probably split in 2 different buffers
    uint g_SurfelGridHashMask;
    hash_map_entry g_SurfelGridHashMap[];
};

bool surfel_grid_hash_find_entry(in ivec3 cell, out uint outIndex)
{
    const uint hash = surfel_grid_cell_hash(g_SurfelGridHeader, cell);

    for (uint i = 0; i < SURFEL_HASH_MAX_PROBES; ++i)
    {
        const uint cellIndex = hash_map_index_probe(hash, i, g_SurfelGridHashMask);

        bool keepSearching;
        if (hash_map_try_find(g_SurfelGridHashMap[cellIndex], hash, keepSearching))
        {
            outIndex = cellIndex;
            return true;
        }

        if (!keepSearching)
        {
            break;
        }
    }

    return false;
}

#endif