oblo_add_library(oblovk)

target_link_libraries(
    oblovk
    PUBLIC
    oblo
    CONAN_PKG::vulkan-headers
    unordered_dense::unordered_dense
    PRIVATE
    CONAN_PKG::vulkan-loader
    CONAN_PKG::vulkan-memory-allocator
    CONAN_PKG::glslang
)