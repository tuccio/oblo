oblo_add_library(oblovk)

target_link_libraries(
    oblovk
    PUBLIC
    oblo
    3rdparty::vulkan-headers
    3rdparty::unordered_dense
    PRIVATE
    3rdparty::vulkan-loader
    3rdparty::vulkan-memory-allocator
    3rdparty::glslang
)