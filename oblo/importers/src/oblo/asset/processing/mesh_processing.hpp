#pragma once

namespace oblo
{
    class mesh;
}

namespace oblo::importers::mesh_processing
{
    bool build_meshlets(const mesh& inputMesh, mesh& outputMesh);
}