oblo_add_library(core)

target_link_libraries(
    oblo_core
    PRIVATE
    xxHash::xxhash
)

find_package(Eigen3 REQUIRED)

target_link_libraries(oblo_test_core PUBLIC Eigen3::Eigen)