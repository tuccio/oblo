#ifndef OBLO_INCLUDE_RENDERER_MESHES_MESH_DEFINES
#define OBLO_INCLUDE_RENDERER_MESHES_MESH_DEFINES

// These need to match oblo::vertex_attributes in C++
#define OBLO_VERTEX_ATTRIBUTE_POSITION 0
#define OBLO_VERTEX_ATTRIBUTE_NORMAL 1
#define OBLO_VERTEX_ATTRIBUTE_TANGENT 2
#define OBLO_VERTEX_ATTRIBUTE_BITANGENT 3
#define OBLO_VERTEX_ATTRIBUTE_UV0 4
#define OBLO_VERTEX_ATTRIBUTE_MAX 8

#define OBLO_MESH_DATA_DRAW_RANGE 0
#define OBLO_MESH_DATA_AABBS 1
#define OBLO_MESH_DATA_MAX 2

#define OBLO_DESCRIPTOR_SET_MESH_DATABASE 0
#define OBLO_BINDING_MESH_DATABASE 33

// These need to match oblo::mesh_index_type
#define OBLO_MESH_DATA_INDEX_TYPE_NONE 0
#define OBLO_MESH_DATA_INDEX_TYPE_U8 1
#define OBLO_MESH_DATA_INDEX_TYPE_U16 2
#define OBLO_MESH_DATA_INDEX_TYPE_U32 3

// These values are currently hardcoded both for the import process and in mesh_database
#define OBLO_MESHLET_MAX_VERTICES 64
#define OBLO_MESHLET_MAX_PRIMITIVES 124

#endif