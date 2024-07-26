# Compile-time XXH32 and XXH64 implementations
# Source: https://github.com/ekpyron/xxhashct.git (commit 2d1d90506d7da2f8f009bef8596540cc7d220c86)
add_library(xxhashct INTERFACE)

target_include_directories(xxhashct INTERFACE include)

add_library(xxhashct::xxhashct ALIAS xxhashct)