#include <meshoptimizer.h>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/data_format.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/scene/assets/mesh.hpp>

namespace oblo::importers::mesh_processing
{
    bool build_meshlets(const mesh& inputMesh, mesh& outputMesh)
    {
        if (inputMesh.get_primitive_kind() != primitive_kind::triangle)
        {
            return false;
        }

        u32 inputIndexCount = inputMesh.get_index_count();

        dynamic_array<u32> generatedIndices;

        if (!inputMesh.has_attribute(attribute_kind::indices))
        {
            inputIndexCount = inputMesh.get_vertex_count() * 3;
            generatedIndices.resize_default(inputIndexCount);

            for (u32 i = 0; i < inputIndexCount; ++i)
            {
                generatedIndices[i] = i;
            }
        }

        // These are the values suggested for NVIDIA cards
        constexpr u8 maxVertices{64u};
        constexpr u8 maxTriangles{124u};

        const auto maxMeshlets = meshopt_buildMeshletsBound(inputIndexCount, maxVertices, maxTriangles);

        dynamic_array<meshopt_Meshlet> meshlets;
        meshlets.resize_default(maxMeshlets);

        dynamic_array<u32> meshletVertices;
        meshletVertices.resize_default(maxMeshlets * maxVertices);

        dynamic_array<u8> microIndices;
        microIndices.resize_default(maxMeshlets * maxTriangles * 3);

        usize numMeshlets;

        const std::span positions = inputMesh.get_attribute<vec3>(attribute_kind::position);

        constexpr f32 coneWeight = 0.f;

        switch (inputMesh.get_attribute_format(attribute_kind::indices))
        {
        case data_format::u16:
            numMeshlets = meshopt_buildMeshlets(meshlets.data(),
                meshletVertices.data(),
                microIndices.data(),
                inputMesh.get_attribute<u16>(attribute_kind::indices).data(),
                inputMesh.get_index_count(),
                reinterpret_cast<const float*>(positions.data()),
                inputMesh.get_vertex_count(),
                sizeof(vec3),
                maxVertices,
                maxTriangles,
                coneWeight);
            break;

        case data_format::u32:
            numMeshlets = meshopt_buildMeshlets(meshlets.data(),
                meshletVertices.data(),
                microIndices.data(),
                inputMesh.get_attribute<u32>(attribute_kind::indices).data(),
                inputMesh.get_index_count(),
                reinterpret_cast<const float*>(positions.data()),
                inputMesh.get_vertex_count(),
                sizeof(vec3),
                maxVertices,
                maxTriangles,
                coneWeight);
            break;

        case data_format::enum_max:
            numMeshlets = meshopt_buildMeshlets(meshlets.data(),
                meshletVertices.data(),
                microIndices.data(),
                generatedIndices.data(),
                generatedIndices.size(),
                reinterpret_cast<const float*>(positions.data()),
                inputMesh.get_vertex_count(),
                sizeof(vec3),
                maxVertices,
                maxTriangles,
                coneWeight);
            break;

        default:
            unreachable();
        }

        if (numMeshlets == 0)
        {
            return false;
        }

        const u32 inAttributesCount = inputMesh.get_attributes_count();

        buffered_array<mesh_attribute, 8> outAttributes;
        outAttributes.reserve(inAttributesCount + 1);

        for (u32 i = 0; i < inAttributesCount; ++i)
        {
            const auto inAttribute = inputMesh.get_attribute_at(i);

            if (!is_vertex_attribute(inAttribute.kind))
            {
                continue;
            }

            outAttributes.push_back(inAttribute);
        }

        // We add the micro-indices for the rasterization pipeline
        // We also add regular indices, which we will use to construct the BLAS
        constexpr u32 numIndicesBuffers = 2;

        outAttributes.push_back({.kind = attribute_kind::microindices, .format = data_format::u8});

        const data_format finalIndicesFormat =
            inputMesh.get_vertex_count() > ~u16() ? data_format::u32 : data_format::u16;
        outAttributes.push_back({.kind = attribute_kind::indices, .format = finalIndicesFormat});

        const auto& lastMeshlet = meshlets[numMeshlets - 1];

        const auto numTotalVertices = lastMeshlet.vertex_offset + lastMeshlet.vertex_count;
        const auto numTotalIndices = 3 * (lastMeshlet.triangle_offset + lastMeshlet.triangle_count);

        outputMesh.allocate(primitive_kind::triangle,
            numTotalVertices,
            numTotalIndices,
            u32(numMeshlets),
            outAttributes);

        const std::span outMeshlets = outputMesh.get_meshlets();

        for (usize i = 0; i < numMeshlets; ++i)
        {
            const auto& currentMeshlet = meshlets[i];

            outMeshlets[i] = {
                .vertexOffset = currentMeshlet.vertex_offset,
                .vertexCount = currentMeshlet.vertex_count,
                .indexOffset = currentMeshlet.triangle_offset,
                .indexCount = currentMeshlet.triangle_count * 3,
            };
        }

        // Shrink so we can use for each
        meshletVertices.resize(numTotalVertices);

        for (const auto& vertexAttribute :
            std::span{outAttributes}.subspan(0, outAttributes.size() - numIndicesBuffers))
        {
            OBLO_ASSERT(is_vertex_attribute(vertexAttribute.kind));

            const std::span src = inputMesh.get_attribute(vertexAttribute.kind);
            const std::span dst = outputMesh.get_attribute(vertexAttribute.kind);

            byte* outIt = dst.data();

            const auto [attributeSize, _] = get_size_and_alignment(vertexAttribute.format);

            for (const u32 vertex : meshletVertices)
            {
                std::memcpy(outIt, src.data() + vertex * attributeSize, attributeSize);
                outIt += attributeSize;
            }
        }

        const auto outMicroIndices = outputMesh.get_attribute<u8>(attribute_kind::microindices);
        OBLO_ASSERT(outMicroIndices.size() == numTotalIndices);
        OBLO_ASSERT(microIndices.size() >= numTotalIndices);

        std::memcpy(outMicroIndices.data(), microIndices.data(), numTotalIndices);

        const auto translateMicroIndices = [&outMeshlets, &microIndices]<typename T>(std::span<T> indices)
        {
            auto outIt = indices.begin();

            for (const auto& meshlet : outMeshlets)
            {
                for (u32 i = meshlet.indexOffset; i < meshlet.indexOffset + meshlet.indexCount; ++i)
                {
                    const T index = narrow_cast<T>(u32(microIndices[i]) + meshlet.vertexOffset);
                    *outIt = index;
                    ++outIt;
                }
            }
        };

        switch (finalIndicesFormat)
        {
        case data_format::u16:
            translateMicroIndices(outputMesh.get_attribute<u16>(attribute_kind::indices));
            break;

        case data_format::u32:
            translateMicroIndices(outputMesh.get_attribute<u32>(attribute_kind::indices));
            break;

        default:
            unreachable();
        }

        return true;
    }
}