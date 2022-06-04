oblo_add_library(oblovk)

target_link_libraries(
    oblovk
    oblo
    CONAN_PKG::vulkan-loader)