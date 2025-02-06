add_library(tinygltf_defines INTERFACE)

add_library(TinyGLTF::Defines ALIAS tinygltf_defines)

target_compile_definitions(
    tinygltf_defines
    INTERFACE
    TINYGLTF_USE_RAPIDJSON_CRTALLOCATOR
    TINYGLTF_USE_RAPIDJSON
    TINYGLTF_NO_EXTERNAL_IMAGE
    TINYGLTF_NO_INCLUDE_RAPIDJSON
)

target_include_directories(tinygltf_defines INTERFACE include)